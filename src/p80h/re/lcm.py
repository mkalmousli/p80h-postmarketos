"""lcm: the stock panel driver (LCM_DRIVER for the ITEMS panel) -> params + an ordered init/suspend/resume sequence."""
import struct

from . import stock, xref
from .emu import Emu

UTIL = {0: "set_reset_pin", 24: "udelay", 32: "mdelay", 88: "dsi_set_cmdq", 72: "dsi_set_cmdq_V3", 80: "dsi_set_cmdq_V2"}
HOOKS = ("set_util_funcs", "get_params", "init", "suspend", "resume")


def driver(name: str) -> dict[str, int]:
    img = stock.image()
    ptr = struct.pack("<Q", xref.address_of(img, name.encode()))
    off = img.index(ptr)
    return {h: struct.unpack_from("<Q", img, off + 8 * (i + 1))[0] for i, h in enumerate(HOOKS)}


def util_base(fn: int) -> int:
    """lcm_util: the global the set_util_funcs hook memcpy()s the util table into (x0 of that memcpy)."""
    log = Emu(fn, 0x80).run()
    return next(args[0] for kind, t, args in log if kind == "bl")


def packets(words: list[int]) -> list[tuple[int, bytes]]:
    """MTK dsi_set_cmdq data: word0 low byte 0x02 = long packet (type, word count, payload in the next words)."""
    w0 = words[0]
    if w0 & 0xFF == 0x02:
        count, kind = w0 >> 16, (w0 >> 8) & 0xFF
        payload = b"".join(struct.pack("<I", w) for w in words[1:])[:count]
        return [(kind, payload)]
    return [((w0 >> 8) & 0xFF, bytes([(w0 >> 16) & 0xFF, (w0 >> 24) & 0xFF]))]


def sequence(fn: int, base: int, names: dict[int, str]) -> list[tuple]:
    out = []
    for kind, target, args in Emu(fn, 0x1000).run():
        if kind == "bl":
            out.append(("call", names.get(target, hex(target)), args[0]) if not names.get(target, "").startswith(("mt_set_gpio",)) else
                       (names[target], args[0] & 0x7FFFFFFF, args[1]))
            continue
        slot = target[1] - base if isinstance(target, tuple) else None
        what = UTIL.get(slot, f"util+{slot}")
        if what == "dsi_set_cmdq":
            n = args[1]
            words = list(struct.unpack(f"<{n}I", args[4][:4 * n]))
            kind_, data = packets(words)[0]
            out.append(("dsi", kind_, data.hex() if kind_ in (0x39, 0x29) else data[: 1 if kind_ == 0x05 else 2].hex()))
        elif what in ("mdelay", "udelay", "set_reset_pin"):
            out.append((what, args[0]))
        else:
            out.append((what,) + tuple(a for a in args[:4]))
    return out


def dump() -> str:
    from .items import items
    names = {}
    for line in stock.OUT.joinpath("symbols.txt").read_text().splitlines():
        a, _, n = line.split()
        names.setdefault(int(a, 16), n)
    panel = items()["LCM"]
    d = driver(panel)
    base = util_base(d["set_util_funcs"])
    lines = [f"panel {panel}"]
    for hook in ("init", "suspend", "resume"):
        lines.append(f"{hook}:")
        lines += [f"  {op}" for op in sequence(d[hook], base, names)]
    return "\n".join(lines)
