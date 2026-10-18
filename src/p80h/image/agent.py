"""Cross-compile p80hd (static arm64) and wire it into the initramfs."""
import shutil
import subprocess
from pathlib import Path

from . import initramfs

AGENT_SRC = Path(__file__).resolve().parents[3] / "agent"


def compile_agent(out: Path) -> Path:
    cc = shutil.which("aarch64-linux-gnu-gcc") or shutil.which("aarch64-linux-musl-gcc")
    if not cc:
        raise SystemExit("need an aarch64 cross compiler (apt install gcc-aarch64-linux-gnu)")
    srcs = sorted(str(p) for p in AGENT_SRC.glob("*.c"))
    subprocess.run([cc, "-static", "-Os", "-Wall", "-o", str(out), *srcs], check=True)
    return out


def install(root: Path, binary: Path, rescue: bool) -> None:
    """Start p80hd right after the USB network comes up; rescue additionally parks init so nothing mounts."""
    dest = root / "sbin" / "p80hd"
    shutil.copy(binary, dest)
    dest.chmod(0o755)
    park = "\nwhile true; do sleep 1; done" if rescue else ""
    initramfs.patch_init(root, "start_unudhcpd", "# no devtmpfs in this kernel: create block/input nodes ourselves\nmdev -s 2>/dev/null\nmkdir -p /dev/input\n(while :; do echo > /dev/watchdog0 2>/dev/null; sleep 4; done) &\nmkdir -p /dev/pts; mount -t devpts devpts /dev/pts 2>/dev/null\n/sbin/p80hd &" + park)
