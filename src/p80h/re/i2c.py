"""i2c: every i2c_register_board_info(bus, info[], n) call in the stock kernel -> (caller, bus, name, addr, irq)."""
import struct

from . import stock

INFO = 56  # sizeof(struct i2c_board_info) on 3.10 arm64 without ACPI: type[20] flags addr pdata archdata of_node irq


def _bl_target(pc: int, insn: int) -> int | None:
    if insn & 0xFC000000 != 0x94000000:
        return None
    imm = insn & 0x03FFFFFF
    return pc + ((imm - (1 << 26) if imm & (1 << 25) else imm) << 2)


def _regs(img: bytes, call: int) -> dict[int, int]:
    """Constant values of x0..x2 at a call, from the straight-line code before it (adrp/add/movz/movn)."""
    regs: dict[int, int] = {}
    for pc in range(call - 4 * 32, call, 4):
        insn = struct.unpack("<I", stock.read(img, pc, 4))[0]
        rd = insn & 0x1F
        if insn & 0x9F000000 == 0x90000000:  # adrp
            imm = ((insn >> 29) & 3) | (((insn >> 5) & 0x7FFFF) << 2)
            imm -= (1 << 21) if imm & (1 << 20) else 0
            regs[rd] = (pc & ~0xFFF) + (imm << 12)
        elif insn & 0x7F800000 == 0x11000000 and (insn >> 5) & 0x1F in regs:  # add imm
            regs[rd] = regs[(insn >> 5) & 0x1F] + (((insn >> 10) & 0xFFF) << (12 if insn & (1 << 22) else 0))
        elif insn & 0x7F800000 == 0x52800000:  # movz
            regs[rd] = ((insn >> 5) & 0xFFFF) << (16 * ((insn >> 21) & 3))
        elif insn & 0x7F800000 == 0x12800000:  # movn
            regs[rd] = ~(((insn >> 5) & 0xFFFF) << (16 * ((insn >> 21) & 3))) & 0xFFFFFFFF
        elif insn & 0xBFC00000 == 0xB9400000 and (insn >> 5) & 0x1F in regs:  # ldr w/x [xn, #imm] of a constant
            size = 8 if insn & (1 << 30) else 4
            addr = regs[(insn >> 5) & 0x1F] + ((insn >> 10) & 0xFFF) * size
            regs[rd] = int.from_bytes(stock.read(img, addr, size), "little")
        elif _bl_target(pc, insn) is not None:
            regs = {k: v for k, v in regs.items() if k >= 19}  # a call clobbers x0..x18, keeps x19..x28
    return regs


def _caller(syms: dict[str, int], pc: int) -> str:
    best = max((a, n) for n, a in syms.items() if a <= pc)
    return best[1]


def board_info() -> list[tuple[str, int, str, int, int]]:
    img, syms = stock.image(), stock.symbols()
    target = syms["i2c_register_board_info"]
    out = []
    for off in range(0, len(img) - 4, 4):
        pc = stock.BASE + off
        if _bl_target(pc, struct.unpack_from("<I", img, off)[0]) != target:
            continue
        r = _regs(img, pc)
        if 1 not in r:
            out.append((_caller(syms, pc), r.get(0, -1), "?", -1, -1))
            continue
        for i in range(r.get(2, 1)):
            raw = stock.read(img, r[1] + i * INFO, INFO)
            name = raw[:20].split(b"\0")[0].decode(errors="replace")
            out.append((_caller(syms, pc), r.get(0, -1), name, struct.unpack_from("<H", raw, 22)[0],
                        struct.unpack_from("<i", raw, 48)[0]))
    return out
