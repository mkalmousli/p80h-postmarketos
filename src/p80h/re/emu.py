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


COND = ("eq", "ne", "hi", "ls", "cs", "hs", "cc", "lo", "ge", "lt", "gt", "le", "mi", "pl")


class Emu:
    def __init__(self, start: int, length: int = 0x2000, args=(), stop=(), descend=None):
        """descend(target) -> True: follow a bl into that function (disassembled on demand) instead of logging it."""
        self.code, self.pc, self.stop, self.descend = disasm(start, start + length), start, set(stop), descend
        self.x = {i: 0 for i in range(31)} | dict(enumerate(args))
        self.sp, self.mem, self.log = 0x7000_0000, {}, []
        self.lr_stack = []

    def _ensure(self, at: int, length: int = 0x1000) -> None:
        if at not in self.code:
            self.code.update(disasm(at, at + length))

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

    def load(self, addr, size: int):
        if not isinstance(addr, int):
            return ("mem", 0)
        if all(addr + i in self.mem for i in range(size)):
            parts = [self.mem[addr + i] for i in range(size)]
            if not all(isinstance(p, int) for p in parts):
                return parts[0]
            return int.from_bytes(bytes(parts), "little")
        if stock.BASE <= addr < stock.BASE + 0x2000000 and addr - stock.BASE + size <= len(stock.image()):
            return int.from_bytes(stock.read(stock.image(), addr, size), "little")
        if stock.BASE + len(stock.image()) <= addr < stock.bss_end():  # .bss: zero until the driver sets it (first open)
            return 0
        return ("mem", addr)

    def store(self, addr, size: int, v) -> None:
        if not isinstance(addr, int):
            return
        for i in range(size):
            self.mem[addr + i] = (v >> (8 * i)) & 0xFF if isinstance(v, int) else v

    def bytes_at(self, addr: int, n: int) -> bytes:
        return bytes(self.mem.get(addr + i, 0) if isinstance(self.mem.get(addr + i, 0), int) else 0 for i in range(n))

    def addr(self, ops: str) -> tuple[int, str | None, int]:
        """Memory operand -> (address, base register to write back, new base)."""
        m = re.match(r"\[(\w+)(?:, ([^\]]+))?\](!)?(?:, #(-?\w+))?", ops)
        base = self.reg(m[1])
        if not isinstance(base, int):
            return ("unknown",), None, base
        off = 0
        if m[2]:
            idx = re.match(r"#(-?\w+)|(\w+)(?:, (\w+)(?: #(\d+))?)?", m[2])
            if idx[1]:
                off = int(idx[1], 0)
            else:
                off = self.reg(idx[2])
                if not isinstance(off, int):
                    return ("unknown",), None, base
                if idx[3] in ("sxtw",) and off & 0x80000000:
                    off -= 1 << 32
                if idx[3] in ("uxtw",):
                    off &= 0xFFFFFFFF
                off <<= int(idx[4] or 0)
        if m[4]:  # post-index
            return base, m[1], base + int(m[4], 0)
        return base + off, m[1] if m[3] else None, base + off

    def _cond(self, c: str) -> bool:
        """Condition of the last cmp/cmn/tst; False when an operand is unknown (fall through)."""
        op, v, b, w = getattr(self, "flags", ("cmp", None, None, False))
        if not (isinstance(v, int) and isinstance(b, int)):
            return False
        bits = 32 if w else 64
        m = (1 << bits) - 1
        if op == "tst":
            r = v & b & m
            return {"eq": r == 0, "ne": r != 0}.get(c, False)
        b = (-b) & m if op == "cmn" else b & m
        v &= m
        sv = v - (1 << bits) if v >> (bits - 1) else v
        sb = b - (1 << bits) if b >> (bits - 1) else b
        return {"eq": v == b, "ne": v != b, "hi": v > b, "ls": v <= b, "cs": v >= b, "hs": v >= b,
                "cc": v < b, "lo": v < b, "ge": sv >= sb, "lt": sv < sb, "gt": sv > sb, "le": sv <= sb,
                "mi": (v - b) & (1 << (bits - 1)) != 0, "pl": (v - b) & (1 << (bits - 1)) == 0}[c]

    def args(self) -> tuple:
        return tuple(self.x[i] for i in range(4))

    def step(self) -> bool:
        if self.pc not in self.code:
            self._ensure(self.pc)
        op, ops = self.code[self.pc]
        a = [s.strip() for s in re.split(r",(?![^\[]*\])", ops)] if ops else []
        nxt = self.pc + 4
        imm = lambda s: int(s.lstrip("#"), 0)
        if op == "mov":
            self.set(a[0], imm(a[1]) if a[1].startswith("#") else self.reg(a[1]))
        elif op == "movk":
            sh = int(a[2].split("#")[1]) if len(a) > 2 else 0
            v = self.reg(a[0])
            self.set(a[0], (v & ~(0xFFFF << sh)) | (imm(a[1]) << sh) if isinstance(v, int) else v)
        elif op in ("add", "sub"):
            b = imm(a[2]) if a[2].startswith("#") else self.reg(a[2])
            if len(a) > 3 and isinstance(b, int):
                ext = a[3].split()[0]
                sh = int(a[3].split("#")[1]) if "#" in a[3] else 0
                if ext in ("sxtb", "uxtb", "sxth", "uxth", "sxtw", "uxtw"):
                    w = {"b": 8, "h": 16, "w": 32}[ext[-1]]
                    b &= (1 << w) - 1
                    if ext[0] == "s" and b >> (w - 1):
                        b -= 1 << w
                b <<= sh
            v = self.reg(a[1])
            if isinstance(v, int) and isinstance(b, int):
                self.set(a[0], v + b if op == "add" else v - b)
            else:
                self.set(a[0], v if not isinstance(v, int) else b)
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
            if self.descend and self.descend(t) and len(self.lr_stack) < 12:
                self.lr_stack.append(nxt)
                self.x[30] = nxt
                self._ensure(t)
                nxt = t
            else:
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
        elif op in ("tbz", "tbnz"):
            v = self.reg(a[0])
            if isinstance(v, int) and ((v >> imm(a[1])) & 1 == 0) == (op == "tbz"):
                nxt = int(a[2], 16)
        elif op in ("cmp", "cmn", "tst"):
            v, b = self.reg(a[1 - 1]), imm(a[1]) if a[1].startswith("#") else self.reg(a[1])
            self.flags = (op, v, b, a[0][0] == "w")
        elif op.startswith("b.") and op[2:] in COND:
            if self._cond(op[2:]):
                nxt = int(a[0], 16)
        elif op == "ret":
            if not self.lr_stack:
                return False
            nxt = self.lr_stack.pop()
        elif op in ("csel", "csinc", "csneg", "csinv", "cset", "csetm", "cinc"):
            if op in ("cset", "csetm"):
                c = self._cond(a[1]); self.set(a[0], (1 if op == "cset" else MASK) if c else 0)
            elif op == "cinc":
                v = self.reg(a[1]); c = self._cond(a[2])
                self.set(a[0], v + 1 if c and isinstance(v, int) else v)
            else:
                c = self._cond(a[3]); x, y = self.reg(a[1]), self.reg(a[2])
                if not c and isinstance(y, int):
                    y = {"csinc": y + 1, "csneg": -y, "csinv": ~y}.get(op, y)
                self.set(a[0], x if c else y)
        elif op in ("lsl", "lsr", "asr", "ror", "eor", "bic", "mul", "udiv", "sdiv", "orn", "eon"):
            x = self.reg(a[1]); y = imm(a[2]) if a[2].startswith("#") else self.reg(a[2])
            if isinstance(x, int) and isinstance(y, int):
                bits = 32 if a[0][0] == "w" else 64
                m = (1 << bits) - 1
                r = {"lsl": lambda: x << (y % bits), "lsr": lambda: (x & m) >> (y % bits),
                     "asr": lambda: ((x & m) - ((x & m) >> (bits - 1) << bits)) >> (y % bits),
                     "ror": lambda: ((x & m) >> (y % bits)) | (x << (bits - y % bits)),
                     "eor": lambda: x ^ y, "bic": lambda: x & ~y, "mul": lambda: x * y,
                     "udiv": lambda: (x & m) // (y & m) if y & m else 0, "sdiv": lambda: int(x / y) if y else 0,
                     "orn": lambda: x | ~y, "eon": lambda: x ^ ~y}[op]()
                self.set(a[0], r)
            else:
                self.set(a[0], ("mem", 0))
        elif op in ("madd", "msub"):
            x, y, z = (self.reg(r) for r in a[1:4])
            self.set(a[0], (z + x * y if op == "madd" else z - x * y) if all(isinstance(v, int) for v in (x, y, z)) else ("mem", 0))
        elif op in ("ubfx", "sbfx", "ubfiz", "sbfiz", "bfi", "bfxil"):
            x = self.reg(a[1]); lsb, w = imm(a[2]), imm(a[3])
            if not isinstance(x, int):
                self.set(a[0], x)
            elif op in ("ubfx", "sbfx"):
                v = (x >> lsb) & ((1 << w) - 1)
                if op == "sbfx" and v >> (w - 1):
                    v -= 1 << w
                self.set(a[0], v)
            elif op in ("ubfiz", "sbfiz"):
                self.set(a[0], (x & ((1 << w) - 1)) << lsb)
            else:
                d = self.reg(a[0]); m = ((1 << w) - 1)
                if isinstance(d, int):
                    self.set(a[0], (d & ~(m << lsb)) | ((x & m) << lsb) if op == "bfi" else (d & ~m) | ((x >> lsb) & m))
        elif op in ("uxtb", "uxth", "sxtb", "sxth", "sxtw", "uxtw"):
            x = self.reg(a[1])
            if isinstance(x, int):
                w = {"b": 8, "h": 16, "w": 32}[op[-1]]
                v = x & ((1 << w) - 1)
                if op[0] == "s" and v >> (w - 1):
                    v -= 1 << w
                self.set(a[0], v)
            else:
                self.set(a[0], x)
        elif op in ("neg", "mvn", "negs"):
            x = self.reg(a[1])
            self.set(a[0], (-x if op != "mvn" else ~x) if isinstance(x, int) else x)
        elif op in ("ldrsw", "ldrsb", "ldrsh", "ldursw", "ldursb", "ldursh", "ldurb", "ldurh", "sturb", "sturh"):
            size = {"w": 4, "b": 1, "h": 2}[op[-1]]
            ad, wb, nb = self.addr(", ".join(a[1:]))
            if op.startswith("st"):
                self.store(ad, size, self.reg(a[0]))
            else:
                v = self.load(ad, size)
                if isinstance(v, int) and "s" in op[3:5] and v >> (8 * size - 1):
                    v -= 1 << (8 * size)
                self.set(a[0], v)
            if wb:
                self.set(wb, nb)
        elif op in ("ands", "adds", "subs", "bics"):
            x = self.reg(a[1]); y = imm(a[2]) if a[2].startswith("#") else self.reg(a[2])
            r = {"ands": lambda: x & y, "adds": lambda: x + y, "subs": lambda: x - y, "bics": lambda: x & ~y}[op]() \
                if isinstance(x, int) and isinstance(y, int) else ("mem", 0)
            self.set(a[0], r)
            self.flags = ("tst", r, MASK, a[0][0] == "w") if op in ("ands", "bics") else ("cmp", x, y, a[0][0] == "w")
        elif op in ("ccmp", "ccmn"):
            self.flags = ("cmp", None, None, False)
        elif op in ("prfm", "hint", "yield", "clrex", "sev", "wfe", "wfi", "brk", "hvc", "smc", "msr", "mrs", "stlr",
                    "ldar", "ldaxr", "stlxr", "ldxr", "stxr", "dc", "ic", "tlbi"):
            if op in ("mrs", "ldar", "ldaxr", "ldxr"):
                self.set(a[0], ("mem", 0))
            if op in ("stlxr", "stxr"):
                self.set(a[0], 0)
        elif op == "adr":
            self.set(a[0], int(a[1], 16))
        elif op in ("umull", "smull", "umulh", "smulh", "umaddl", "smaddl"):
            x, y = self.reg(a[1]), self.reg(a[2])
            if isinstance(x, int) and isinstance(y, int):
                r = (x * y) >> 64 if op.endswith("h") else x * y
                if op.endswith("addl"):
                    z = self.reg(a[3]); r = r + z if isinstance(z, int) else z
                self.set(a[0], r)
            else:
                self.set(a[0], ("mem", 0))
        elif op in ("br", "blr") and False:
            pass
        elif op == "br":
            t = self.reg(a[0])
            if not isinstance(t, int):
                raise NotImplementedError(f"{self.pc:#x}: br through unknown {t}")
            self._ensure(t)
            nxt = t
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
