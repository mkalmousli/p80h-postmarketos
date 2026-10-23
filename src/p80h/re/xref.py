"""xref: code locations in the stock kernel that build the address of a string or symbol (adrp + add)."""
import struct

from . import stock


def address_of(img: bytes, text: bytes) -> int:
    return stock.BASE + img.index(text + b"\0")


def xrefs(target: int) -> list[int]:
    img = stock.image()
    words = struct.unpack(f"<{len(img) // 4}I", img[:len(img) // 4 * 4])
    out = []
    for i, insn in enumerate(words):
        if insn & 0x9F000000 != 0x90000000:
            continue
        pc = stock.BASE + 4 * i
        imm = ((insn >> 29) & 3) | (((insn >> 5) & 0x7FFFF) << 2)
        page = (pc & ~0xFFF) + ((imm - (1 << 21) if imm & (1 << 20) else imm) << 12)
        if page != target & ~0xFFF:
            continue
        for j in range(i + 1, min(i + 8, len(words))):
            add = words[j]
            if add & 0xFF800000 == 0x91000000 and (add >> 5) & 0x1F == insn & 0x1F and page + ((add >> 10) & 0xFFF) == target:
                out.append(stock.BASE + 4 * j)
    return out
