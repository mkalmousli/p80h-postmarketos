"""Cross-compile p80hd (static arm64) and wire it into the initramfs."""
import shutil
import subprocess
from pathlib import Path

from . import initramfs

CHARGE_SRC = Path(__file__).resolve().parents[3] / "src" / "charge"
CLOCK_SRC = Path(__file__).resolve().parents[3] / "src" / "clock"
BOOT_SRC = Path(__file__).resolve().parents[3] / "src" / "boot"
DAEMON_SRC = Path(__file__).resolve().parents[3] / "src" / "daemon"


def compile_daemon(out: Path) -> Path:
    cc = shutil.which("aarch64-linux-gnu-gcc") or shutil.which("aarch64-linux-musl-gcc")
    if not cc:
        raise SystemExit("need an aarch64 cross compiler (apt install gcc-aarch64-linux-gnu)")
    srcs = sorted(str(p) for p in DAEMON_SRC.glob("*.c"))
    subprocess.run([cc, "-static", "-Os", "-Wall", "-o", str(out), *srcs], check=True)
    return out


def compile_charge(out: Path, src: Path = CHARGE_SRC / "charge.c") -> Path:
    """one C file, or every .c of a directory (the clock)"""
    cc = shutil.which("aarch64-linux-gnu-gcc") or shutil.which("aarch64-linux-musl-gcc")
    srcs = sorted(map(str, src.glob("*.c"))) if src.is_dir() else [str(src)]
    subprocess.run([cc, "-static", "-O2", "-Wall", "-o", str(out), *srcs, "-lm"], check=True)
    return out


def install(root: Path, binary: Path, rescue: bool, wait: bool = False, clock: bool = False) -> None:
    """Start p80hd right after the USB network comes up; rescue additionally parks init so nothing mounts."""
    dest = root / "sbin" / "p80hd"
    shutil.copy(binary, dest)
    dest.chmod(0o755)
    splash = root / "sbin" / "p80h-splash"  # quiet animated boot screen; the desktop session stops it before X
    shutil.copy(compile_charge(binary.parent / "p80h-splash", BOOT_SRC / "splash.c"), splash)
    splash.chmod(0o755)
    park = "\nwhile true; do sleep 1; done" if rescue else ""
    if clock:  # the tablet is a clock: never mount the rootfs, the clock owns the screen and the power key
        dest_clock = root / "sbin" / "p80h-clock"
        shutil.copy(compile_charge(binary.parent / "p80h-clock", CLOCK_SRC), dest_clock)
        dest_clock.chmod(0o755)
        park = "\nwhile true; do /sbin/p80h-clock; sleep 1; done"
    elif wait:  # charge-only: p80h-charge screen until "Continue"; rootfs mounts after it, else screen off and park
        charge = root / "sbin" / "p80h-charge"
        shutil.copy(compile_charge(binary.parent / "p80h-charge"), charge)
        charge.chmod(0o755)
        park = ("\n/sbin/p80h-charge"
                "\nif [ $? -ne 0 ]; then"
                "\n  for c in 1 2 3; do echo 0 > /sys/devices/system/cpu/cpu$c/online; done 2>/dev/null"
                "\n  echo 0 > /sys/class/backlight/backlight/brightness 2>/dev/null"
                "\n  while true; do sleep 60; done"
                "\nfi")
    initramfs.patch_init(root, "start_unudhcpd", "# no devtmpfs in this kernel: create block/input nodes ourselves\nmdev -s 2>/dev/null\n/sbin/p80h-splash &\nmkdir -p /dev/input\nmkdir -p /dev/pts; mount -t devpts devpts /dev/pts 2>/dev/null\n/sbin/p80hd &\n(for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do sleep 2; echo \"p80h-dbg $(ip -o addr show usb0 2>&1 | head -2) daemon=$(pidof p80hd) $(netstat -ltn 2>&1 | grep 4444)\" > /dev/kmsg; done) &" + park)
