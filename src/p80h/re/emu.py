"""emu: run straight-line stock kernel code (objdump text) and record the calls it makes.

Enough AArch64 for vendor init routines (LCM init tables built on the stack, util callbacks):
mov/movk/movn/orr, add/sub imm, adrp, and, ldr/str/stp/ldp/strb/strh/stur, bl, blr, b, cbz/cbnz, ret.
Unknown memory reads return symbolic ('mem', addr); blr through such a value is logged as ('call', addr).
"""
import re
import subprocess

from . import stock

MASK = (1 << 64) - 1


def disasm(start: int, end: int) -> dict[int, tuple[str, str]]:
    out = subprocess.run(["aarch64-linux-gnu-objdump", "-d", "--no-show-raw-insn", f"--start-address={start:#x}",
                          f"--stop-address={end:#x}", str(stock.elf())], check=True, capture_output=True, text=True).stdout
    code = {}
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(\S+)\s*(.*)", line)
        if m:
            code[int(m[1], 16)] = (m[2], m[3].split("//")[0].split("<")[0].strip())
    return code


class Emu:
    def __init__(self, start: int, length: int = 0x2000, args=(), stop=()):
        self.code, self.pc, self.stop = disasm(start, start + length), start, set(stop)
        self.x = {i: 0 for i in range(31)} | dict(enumerate(args))
        self.sp, self.mem, self.log = 0x7000_0000, {}, []
        self.lr_stack = []

    def reg(self, r: str):
        if r in ("wzr", "xzr"):
            return 0
        if r == "sp":
            return self.sp
        v = self.x[int(r[1:])]
        return v & 0xFFFFFFFF if r[0] == "w" and isinstance(v, int) else v

    def set(self, r: str, v) -> None:
        if r == "sp":
            self.sp = v
        elif r not in ("wzr", "xzr"):
            self.x[int(r[1:])] = (v & (0xFFFFFFFF if r[0] == "w" else MASK)) if isinstance(v, int) else v

    def load(self, addr: int, size: int):
        if all(addr + i in self.mem for i in range(size)):
            parts = [self.mem[addr + i] for i in range(size)]
            if not all(isinstance(p, int) for p in parts):
                return parts[0]
            return int.from_bytes(bytes(parts), "little")
        if stock.BASE <= addr < stock.BASE + 0x2000000 and addr - stock.BASE + size <= len(stock.image()):
            return int.from_bytes(stock.read(stock.image(), addr, size), "little")
        return ("mem", addr)

    def store(self, addr: int, size: int, v) -> None:
        for i in range(size):
            self.mem[addr + i] = (v >> (8 * i)) & 0xFF if isinstance(v, int) else v

    def bytes_at(self, addr: int, n: int) -> bytes:
        return bytes(self.mem.get(addr + i, 0) if isinstance(self.mem.get(addr + i, 0), int) else 0 for i in range(n))

    def addr(self, ops: str) -> tuple[int, str | None, int]:
        """Memory operand -> (address, base register to write back, new base)."""
        m = re.match(r"\[(\w+)(?:, ([^\]]+))?\](!)?(?:, #(-?\w+))?", ops)
        base = self.reg(m[1])
        off = 0
        if m[2]:
            idx = re.match(r"#(-?\w+)|(\w+)(?:, (\w+)(?: #(\d+))?)?", m[2])
            if idx[1]:
                off = int(idx[1], 0)
            else:
                off = self.reg(idx[2])
                if idx[3] in ("sxtw",) and off & 0x80000000:
                    off -= 1 << 32
                off <<= int(idx[4] or 0)
        if m[4]:  # post-index
            return base, m[1], base + int(m[4], 0)
        return base + off, m[1] if m[3] else None, base + off

    def args(self) -> tuple:
        return tuple(self.x[i] for i in range(4))

    def step(self) -> bool:
        op, ops = self.code[self.pc]
        a = [s.strip() for s in re.split(r",(?![^\[]*\])", ops)] if ops else []
        nxt = self.pc + 4
        imm = lambda s: int(s.lstrip("#"), 0)
        if op == "mov":
            self.set(a[0], imm(a[1]) if a[1].startswith("#") else self.reg(a[1]))
        elif op == "movk":
            sh = int(a[2].split("#")[1]) if len(a) > 2 else 0
            v = self.reg(a[0])
            self.set(a[0], (v & ~(0xFFFF << sh)) | (imm(a[1]) << sh))
        elif op in ("add", "sub"):
            b = imm(a[2]) if a[2].startswith("#") else self.reg(a[2])
            if len(a) > 3 and "lsl" in a[3]:
                b <<= int(a[3].split("#")[1])
            v = self.reg(a[1])
            self.set(a[0], v + b if op == "add" else v - b) if isinstance(v, int) else self.set(a[0], v)
        elif op in ("and", "orr"):
            v, b = self.reg(a[1]), imm(a[2]) if a[2].startswith("#") else self.reg(a[2])
            self.set(a[0], (v & b if op == "and" else v | b) if isinstance(v, int) and isinstance(b, int) else v)
        elif op == "adrp":
            self.set(a[0], int(a[1], 16))
        elif op in ("str", "stur", "strb", "strh", "ldr", "ldur", "ldrb", "ldrh"):
            size = {"b": 1, "h": 2}.get(op[-1], 8 if a[0][0] == "x" else 4)
            ad, wb, nb = self.addr(", ".join(a[1:]))
            if op.startswith("st"):
                self.store(ad, size, self.reg(a[0]))
            else:
                self.set(a[0], self.load(ad, size))
            if wb:
                self.set(wb, nb)
        elif op in ("stp", "ldp"):
            size = 8 if a[0][0] == "x" else 4
            ad, wb, nb = self.addr(", ".join(a[2:]))
            for i, r in enumerate(a[:2]):
                self.store(ad + i * size, size, self.reg(r)) if op == "stp" else self.set(r, self.load(ad + i * size, size))
            if wb:
                self.set(wb, nb)
        elif op == "bl":
            t = int(a[0], 16)
            self.log.append(("bl", t, self.args()))
            self.x[0] = 0
        elif op == "blr":
            x0 = self.x[0]
            data = self.bytes_at(x0, 512) if isinstance(x0, int) and 0x6000_0000 <= x0 < 0x7000_0000 else b""
            self.log.append(("blr", self.reg(a[0]), self.args() + (data,)))
            self.x[0] = 0
        elif op == "b":
            nxt = int(a[0], 16)
        elif op in ("cbz", "cbnz"):
            v = self.reg(a[0])
            if isinstance(v, int) and (v == 0) == (op == "cbz"):
                nxt = int(a[1], 16)
        elif op == "ret":
            return False
        elif op in ("nop", "dmb", "dsb", "isb"):
            pass
        else:
            raise NotImplementedError(f"{self.pc:#x}: {op} {ops}")
        self.pc = nxt
        return self.pc not in self.stop

    def run(self, limit: int = 100000) -> list:
        for _ in range(limit):
            if not self.step():
                break
        return self.log
