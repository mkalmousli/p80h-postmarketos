"""stock: decompressed stock kernel + a symbolised ELF of it (kallsyms recovered by vmlinux-to-elf), cached in local/re/stock."""
import subprocess
import zlib
from pathlib import Path

BOOT = Path("local/dumps/emmc-partitions/boot.bin")
OUT = Path("local/re/stock")
BASE = 0xFFFFFFC000080000  # link address of Image offset 0 (vmlinux-to-elf finds the same)
TOOL = "git+https://github.com/marin-m/vmlinux-to-elf@80a92677787662a6fc2eb43b716d75aa7ebda285"


def image() -> bytes:
    path = OUT / "Image"
    if not path.exists():
        raw = BOOT.read_bytes()
        OUT.mkdir(parents=True, exist_ok=True)
        path.write_bytes(zlib.decompressobj(31).decompress(raw[raw.find(b"\x1f\x8b\x08"):]))
    return path.read_bytes()


def elf() -> Path:
    path = OUT / "vmlinux.elf"
    if not path.exists():
        image()
        subprocess.run(["uvx", "--from", TOOL, "vmlinux-to-elf", str(OUT / "Image"), str(path)], check=True)
    return path


def symbols() -> dict[str, int]:
    path = OUT / "symbols.txt"
    if not path.exists():
        nm = subprocess.run(["aarch64-linux-gnu-nm", str(elf())], check=True, capture_output=True, text=True).stdout
        path.write_text(nm)
    out = {}
    for line in path.read_text().splitlines():
        addr, _, name = line.split(maxsplit=2)
        out.setdefault(name, int(addr, 16))
    return out


def read(img: bytes, va: int, n: int) -> bytes:
    return img[va - BASE:va - BASE + n]
