"""MTK boot.img: stock kernel + ramdisk wrapped in the 512-byte "ROOTFS" header LK insists on."""
import struct
import subprocess
import tempfile
from pathlib import Path

MAGIC = 0x58881688


def mtk_ramdisk(gz: bytes) -> bytes:
    return struct.pack("<II32s", MAGIC, len(gz), b"ROOTFS").ljust(512, b"\xff") + gz


def build(gz_ramdisk: bytes, out: Path, stock: Path, cmdline_extra: str = "") -> None:
    """stock/ holds bootimg.cfg and zImage.stock, both extracted from the device's own boot partition."""
    cfg = (stock / "bootimg.cfg").read_text()
    with tempfile.TemporaryDirectory() as d:
        rd, cfg_file = Path(d, "rd.mtk"), Path(d, "bootimg.cfg")
        rd.write_bytes(mtk_ramdisk(gz_ramdisk))
        cfg_file.write_text("\n".join(f"{line} {cmdline_extra}" if line.startswith("cmdline") and cmdline_extra
                                      else line for line in cfg.split("\n")))
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["abootimg", "--create", str(out), "-f", str(cfg_file), "-k", str(stock / "zImage.stock"),
                        "-r", str(rd)], check=True, capture_output=True)
