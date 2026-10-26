"""deploy: test a wrapped kernel one-shot from recovery, promote it to boot only when the automatic checks pass.

LK clears the RTC recovery request when it uses it (2026-10-25: a panic in the recovery kernel came back in the boot
kernel), so a candidate that hangs, panics or misses its checks falls back to the known-good boot image by itself:
no key combos, no adb. Invariant: boot and recovery only ever receive images listed in VERIFIED.
"""
import hashlib
import re
import time
import zlib
from pathlib import Path

from .channels.daemon import DaemonChannel

VERIFIED = Path("local/out/verified.txt")  # sha256 of every image that passed deploy
GOOD = Path("local/out/good-boot.img")  # copy of the boot partition taken before each deploy
PART_MAX = 16 * 1024 * 1024
CHECKS = {  # name: (shell command, predicate on its output)
    "4 cores": ("nproc", lambda o: o.strip() == "4"),
    "rootfs": ("grep -c ' / ext4' /proc/mounts", lambda o: o.strip() == "1"),
    "panel": ("cat /sys/class/drm/card0-DSI-1/status", lambda o: "connected" in o),
    "touch": ("grep -c silead /proc/bus/input/devices", lambda o: o.strip() != "0"),
    "sound": ("cat /proc/asound/cards", lambda o: "P80h" in o),
    "zram": ("cat /proc/swaps", lambda o: "zram" in o),
    "wifi": ("ls /sys/class/net", lambda o: "wlan0" in o),
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verified(data: bytes) -> bool:
    return VERIFIED.exists() and sha(data) in VERIFIED.read_text().split()


def version_of(image: bytes) -> str:
    """'#177 SMP <date>' as uname -v prints it, from the gzip'd kernel inside the wrapped image."""
    kernel = zlib.decompressobj(31).decompress(image[image.find(b"\x1f\x8b\x08"):])
    m = re.search(rb"Linux version \S+ \(.*?\) (#\d+ SMP [^\n\x00]*?\d{4})", kernel)
    return m[1].decode() if m else ""


def _part(ch: DaemonChannel, name: str) -> str:
    return "/dev/" + ch.sh(f"grep -l PARTNAME={name}$ /sys/class/block/mmcblk*p*/uevent | head -1").split("/")[4]


def _write(ch: DaemonChannel, name: str, data: bytes) -> None:
    if not 0 < len(data) <= PART_MAX:
        raise SystemExit(f"{len(data)} bytes do not fit {name}")
    node = _part(ch, name)
    ch.put(node, data)
    if ch.get(node, 0, len(data)) != data:
        raise SystemExit(f"readback of {name} differs")


def _wait_up(secs: float) -> DaemonChannel | None:
    end = time.time() + secs
    while time.time() < end:
        try:
            ch = DaemonChannel()
            if ch.sh("uname -v", timeout=5).strip():
                return ch
        except Exception:
            pass
        time.sleep(3)
    return None


def deploy(image: Path, boot_secs: float = 180) -> str:
    data, ch = image.read_bytes(), DaemonChannel()
    want = version_of(data)
    good = ch.get(_part(ch, "boot"), 0, PART_MAX)
    GOOD.write_bytes(good)
    _write(ch, "recovery", data)
    ch.sh("mkdir -p /var/lib/p80h; touch /var/lib/p80h/candidate; sync")  # 00-candidate.start: reboot if unconfirmed
    ch.reboot("recovery")
    time.sleep(15)
    ch = _wait_up(boot_secs)
    if not ch:
        return "FAILED: tablet silent (the watchdog should bring the boot kernel back; check `p80h status`)"
    got = ch.sh("uname -v").strip()
    if got != want:
        _write(ch, "recovery", good)
        return f"FAILED: candidate {want} did not come up, back on {got}; recovery restored"
    end = time.time() + 120  # the daemon answers before OpenRC has started everything
    while (failed := [n for n, (cmd, ok) in CHECKS.items() if not ok(ch.sh(cmd, timeout=20))]) and time.time() < end:
        time.sleep(5)
    if failed:
        _write(ch, "recovery", good)
        ch.reboot("normal")  # back to the known-good boot image: nothing was promoted
        return f"FAILED checks {failed} on {want}: rebooting into the boot image"
    ch.sh("touch /run/p80h-confirmed")
    _write(ch, "boot", data)
    _write(ch, "recovery", good)  # the previous good image becomes the one-shot rescue
    with VERIFIED.open("a") as f:
        f.write(f"{sha(data)} {image.name} {want}\n")
    return f"deployed {image.name} ({want}): checks {list(CHECKS)} passed; previous boot image kept in recovery"
