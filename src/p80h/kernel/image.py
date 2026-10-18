"""Build the kernel and wrap it the way the MTK bootloader wants: 512-byte KERNEL header + Image.gz + dtb."""
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


def build(initramfs: Path | None, out: Path) -> Path:
    tree.fetch()
    tree.overlay()
    tree.configure()
    src = initramfs.resolve() if initramfs else firstlight_list()
    tree.run("scripts/config", "--file", tree.OUT / ".config", "--set-str", "INITRAMFS_SOURCE", src, cwd=tree.SRC)
    tree.run(*tree.MAKE, "olddefconfig", capture_output=True)
    tree.run(*tree.MAKE, "Image.gz", "mediatek/mt8163-teclast-p80h.dtb")
    boot = tree.OUT / "arch/arm64/boot"
    out.write_bytes(kernel_blob((boot / "Image.gz").read_bytes(), (boot / "dts/mediatek/mt8163-teclast-p80h.dtb").read_bytes()))
    return out
