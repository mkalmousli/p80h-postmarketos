"""Fetch and prepare the mainline source tree. Everything we add lives in kernel/ and is copied in."""
import os
import shutil
import subprocess
from pathlib import Path

TAG = "v6.12.99"
URL = "https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git"
REPO = Path(__file__).resolve().parents[3]
KDIR = REPO / "kernel"
CACHE = Path(os.environ.get("P80H_CACHE", Path.home() / ".cache" / "p80h"))
SRC = Path(os.environ.get("P80H_LINUX", CACHE / "linux"))
OUT = CACHE / "build"
DTS = SRC / "arch/arm64/boot/dts/mediatek"
MAKE = ["make", "-C", str(SRC), "O=" + str(OUT), "ARCH=arm64", "CROSS_COMPILE=aarch64-linux-gnu-", f"-j{os.cpu_count()}"]


def run(*cmd, **kw):
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def fetch() -> None:
    if not SRC.exists():
        run("git", "clone", "--depth", "1", "--branch", TAG, URL, SRC)


def overlay() -> None:
    """Idempotent: copy dts and driver files in, apply patches that are not applied yet."""
    for f in (KDIR / "dts").glob("*"):
        shutil.copy(f, DTS / f.name)
    for f in (KDIR / "drivers").rglob("*"):
        if f.is_file():
            dest = SRC / f.relative_to(KDIR / "drivers")
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(f, dest)
    makefile = DTS / "Makefile"
    line = "dtb-$(CONFIG_ARCH_MEDIATEK) += mt8163-teclast-p80h.dtb\n"
    if line not in makefile.read_text():
        makefile.write_text(makefile.read_text() + line)
    for p in sorted((KDIR / "patches").glob("*.patch")):
        if subprocess.run(["git", "-C", SRC, "apply", "--check", "-R", p], capture_output=True).returncode:
            run("git", "-C", SRC, "apply", p)


def configure() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    run(*MAKE, "allnoconfig")
    run(SRC / "scripts/kconfig/merge_config.sh", "-m", "-O", OUT, OUT / ".config", KDIR / "config/p80h.fragment",
        cwd=SRC, capture_output=True)
    run(*MAKE, "olddefconfig", capture_output=True)
