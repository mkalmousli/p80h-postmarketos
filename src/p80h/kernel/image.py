"""Build the kernel and wrap it the way the MTK bootloader wants: 512-byte KERNEL header + Image.gz + dtb."""
import shutil
import struct
import subprocess
from pathlib import Path

from . import tree

MAGIC = 0x58881688


def kernel_blob(image_gz: bytes, dtb: bytes) -> bytes:
    body = image_gz + dtb
    # the stock image stores the size of header + payload; mirrored here
    return struct.pack("<II32s", MAGIC, len(body) + 512, b"KERNEL").ljust(512, b"\xff") + body


def firstlight_list() -> Path:
    """gen_init_cpio spec: a static init that prints to the console. No root needed to build it."""
    exe = tree.OUT / "firstlight-init"
    tree.run("aarch64-linux-gnu-gcc", "-static", "-Os", "-o", exe, tree.KDIR / "firstlight/init.c")
    spec = tree.OUT / "firstlight.list"
    spec.write_text(f"dir /dev 755 0 0\ndir /proc 755 0 0\nnod /dev/console 600 0 0 c 5 1\nfile /init {exe} 755 0 0\n")
    return spec


def initramfs_dir(init: Path, failsafe: int = 0, rescue: bool = False) -> Path:
    """pmOS initramfs unpacked next to the build, with p80hd wired in; the kernel packs the directory itself."""
    from ..image import daemon, initramfs
    root = tree.OUT / "initramfs-root"
    shutil.rmtree(root, ignore_errors=True)
    initramfs.unpack(init.resolve(), root)
    daemon.install(root, daemon.compile_daemon(tree.OUT / "p80hd"), rescue=rescue)
    initramfs.patch_init(root, "start_unudhcpd", "echo 1 > /sys/module/mtk_sd/parameters/go  # start eMMC enumeration once USB is up")
    if failsafe:  # dev builds: hardware-reset back into the known-good boot partition if nobody takes over
        initramfs.patch_init(root, "start_unudhcpd", f"(sleep {failsafe}; echo b > /proc/sysrq-trigger) &")
    return root


def build(initramfs: Path | None, out: Path, failsafe: int = 0, rescue: bool = False) -> Path:
    tree.fetch()
    tree.overlay()
    tree.configure()
    if initramfs:  # the kernel opens /dev/console before init runs and the pmOS tree ships an empty /dev
        nodes = tree.OUT / "nodes.list"
        nodes.write_text("nod /dev/console 600 0 0 c 5 1\nnod /dev/null 666 0 0 c 1 3\n")
        src = f"{initramfs_dir(initramfs, failsafe, rescue)} {nodes}"
    else:
        src = firstlight_list()
    tree.run("scripts/config", "--file", tree.OUT / ".config", "--set-str", "INITRAMFS_SOURCE", src, cwd=tree.SRC)
    tree.run(*tree.MAKE, "olddefconfig", capture_output=True)
    tree.run(*tree.MAKE, "Image.gz", "mediatek/mt8163-teclast-p80h.dtb")
    boot = tree.OUT / "arch/arm64/boot"
    out.write_bytes(kernel_blob((boot / "Image.gz").read_bytes(), (boot / "dts/mediatek/mt8163-teclast-p80h.dtb").read_bytes()))
    return out
