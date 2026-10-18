"""try-kernel: flash a kernel image into recovery and boot it, without touching the working boot partition."""
import shutil
import time
from pathlib import Path

from . import look
from .channels.daemon import DaemonChannel


def look_ok() -> bool:
    return "	device" in look.adb("devices")


def _daemon_up() -> DaemonChannel | None:
    ch = DaemonChannel()
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


def kernel_of(ch: DaemonChannel) -> str:
    return ch.sh("uname -r").strip()


def read_log(ch: DaemonChannel) -> str:
    """Mainline log from the persistent RAM console (see mt8163-ramlog.c); empty if the stock kernel has none."""
    ch.sh("cat /proc/last_kmsg > /tmp/p80h.lk 2>/dev/null; true")
    raw = ch.get("/tmp/p80h.lk")
    at = raw.find(b"DBGC")  # signature of the vendor header the mainline side wrote
    while at >= 0 and int.from_bytes(raw[at + 40:at + 44], "little") != 0x10000:
        at = raw.find(b"DBGC", at + 1)
    if at < 0:
        return ""
    head, ring = raw[at:at + 512], raw[at + 512:at + 0x10000]
    pos, valid = (int.from_bytes(head[o:o + 4], "little") for o in (52, 56))
    text = ring[pos:valid] + ring[:pos] if valid > pos and valid >= len(ring) else ring[:valid]
    return text.decode(errors="replace")


def emmc_node(ch: DaemonChannel) -> str:
    return "/dev/" + ch.sh("grep -l PARTNAME=expdb$ /sys/class/block/mmcblk0p*/uevent").split("/")[4]


def read_emmc_log(ch: DaemonChannel) -> str:
    """Mainline log written by mt8163-emmclog.c into the first 256 KiB of the expdb partition."""
    ch.sh("mdev -s 2>/dev/null; true")
    raw = ch.get(emmc_node(ch), 0, 512 * 512)
    return raw.replace(b"\0", b"").decode(errors="replace")


def try_kernel(image: Path, boot_secs: float = 90) -> str:
    """Returns the kernel release the tablet ended up running; the caller compares it to the stock one."""
    ch = _wait(_daemon_up, 300)
    if not ch:
        raise SystemExit("no daemon: power-cycle the tablet so it boots the working boot partition")
    stock = kernel_of(ch)
    ch.sh("mdev -s 2>/dev/null; true")
    ch.put(emmc_node(ch), bytes(512 * 512))  # clear the log area so stale data is never mistaken for output
    node = "/dev/" + ch.sh("grep -l PARTNAME=recovery$ /sys/class/block/mmcblk0p*/uevent").split("/")[4]
    ch.put(node, image.read_bytes())
    ch.reboot("recovery")
    if shutil.which("adb") and look_ok():
        look.snap(Path("local/out/screen.txt"), 55)
    if not _wait(lambda: not _daemon_up(), 30):
        return "FAILED (tablet did not reboot)"
    t0 = time.time()
    new = _wait(_daemon_up, boot_secs)
    if new:
        rel = kernel_of(new)
        log = read_emmc_log(new) if rel == stock else ""
        Path("local/out").mkdir(exist_ok=True)
        Path("local/out/last-boot.log").write_text(log)
        return f"{rel} (up after {time.time() - t0:.0f}s), log {len(log)} bytes -> local/out/last-boot.log"
    # nothing came up: the watchdog should reset into the stock boot, then the old daemon answers again
    back = _wait(_daemon_up, 120)
    return f"FAILED (back on {kernel_of(back)})" if back else "FAILED (tablet silent, power-cycle it)"
