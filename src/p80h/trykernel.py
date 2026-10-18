"""try-kernel: flash a kernel image into recovery and boot it, without touching the working boot partition."""
import shutil
import time
from pathlib import Path

from . import look
from .channels.agent import AgentChannel


def look_ok() -> bool:
    return "	device" in look.adb("devices")


def _agent_up() -> AgentChannel | None:
    ch = AgentChannel()
    try:
        return ch if ch.available() else None
    except OSError:
        return None


def _wait(pred, secs: float, step: float = 0.3):
    end = time.time() + secs
    while time.time() < end:
        got = pred()
        if got:
            return got
        time.sleep(step)
    return None


def kernel_of(ch: AgentChannel) -> str:
    return ch.sh("uname -r").strip()


def read_log(ch: AgentChannel) -> str:
    """Mainline log that the ramlog console left in the stock framebuffer's spare page (empty if none)."""
    ch.sh("mdev -s 2>/dev/null; true")
    ch.sh("dd if=/dev/fb0 of=/tmp/p80h.log bs=1048576 skip=4 count=1 2>/dev/null; true")
    page = ch.get("/tmp/p80h.log")
    if page[:7] != b"P80HLOG":
        return ""
    return page[16:16 + int.from_bytes(page[8:12], "little")].decode(errors="replace")


def try_kernel(image: Path, boot_secs: float = 90) -> str:
    """Returns the kernel release the tablet ended up running; the caller compares it to the stock one."""
    ch = _wait(_agent_up, 300)
    if not ch:
        raise SystemExit("no agent: power-cycle the tablet so it boots the working boot partition")
    stock = kernel_of(ch)
    ch.sh("mdev -s 2>/dev/null; true")
    node = "/dev/" + ch.sh("grep -l PARTNAME=recovery$ /sys/class/block/mmcblk0p*/uevent").split("/")[4]
    ch.put(node, image.read_bytes())
    ch.reboot("recovery")
    if shutil.which("adb") and look_ok():
        look.snap(Path("out/screen.txt"), 40)
    if not _wait(lambda: not _agent_up(), 30):
        return "FAILED (tablet did not reboot)"
    t0 = time.time()
    new = _wait(_agent_up, boot_secs)
    if new:
        rel = kernel_of(new)
        log = read_log(new) if rel == stock else ""
        Path("out").mkdir(exist_ok=True)
        Path("out/last-boot.log").write_text(log)
        return f"{rel} (up after {time.time() - t0:.0f}s), log {len(log)} bytes -> out/last-boot.log"
    # nothing came up: the watchdog should reset into the stock boot, then the old agent answers again
    back = _wait(_agent_up, 120)
    return f"FAILED (back on {kernel_of(back)})" if back else "FAILED (tablet silent, power-cycle it)"
