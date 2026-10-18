"""touchfw: cut the Silead GSLx68x firmware for this panel (C805G_HK) out of the stock kernel in boot.bin (stays in local/)."""
import struct
import zlib
from pathlib import Path

BOOT = Path("local/dumps/emmc-partitions/boot.bin")
OUT = Path("local/out/gsl1680-teclast-p80h.fw")
START = 0x96EA10  # 13 concatenated (offset, value) arrays in the decompressed stock kernel, one per panel model
COUNT = 60885
INDEX = 6  # C805G_HK: order of the GSLX68X_FW_* name strings next to the arrays


def extract() -> Path:
    raw = BOOT.read_bytes()
    kernel = zlib.decompressobj(31).decompress(raw[raw.find(b"\x1f\x8b\x08"):])
    pairs = [struct.unpack_from("<II", kernel, START + 8 * i) for i in range(COUNT)]
    starts = [i for i, p in enumerate(pairs) if p == (0xF0, 2)] + [COUNT]
    fw = pairs[starts[INDEX]:starts[INDEX + 1]]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(b"".join(struct.pack("<II", *p) for p in fw))
    return OUT
