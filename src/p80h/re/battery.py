"""battery: the stock fuel-gauge OCV profile (BATTERY_PROFILE_STRUC {s32 dod; s32 mV}[]) -> DT simple-battery table.

fgauge_get_profile(temp) returns one of five tables via `adrp; add; add` (one per TEMPERATURE_T*); on this board all
five are the same table. Q_MAX (3000 mAh, all temperatures) is a movz in __batt_meter_init_cust_data_from_cust_header.
"""
import struct

from . import stock

ENTRY, MAX_ENTRIES = 8, 103  # tables are 0x338 bytes apart


def _tables(img: bytes) -> list[int]:
    fn = stock.symbols()["fgauge_get_profile"]
    out, base = [], None
    for pc in range(fn, fn + 0x8c, 4):
        insn = struct.unpack("<I", stock.read(img, pc, 4))[0]
        if insn & 0x9F000000 == 0x90000000 and insn & 0x1F == 0:  # adrp x0
            imm = ((insn >> 29) & 3) | (((insn >> 5) & 0x7FFFF) << 2)
            base = (pc & ~0xFFF) + ((imm - (1 << 21) if imm & (1 << 20) else imm) << 12)
        elif insn & 0xFFC003FF == 0x91000000 and base is not None:  # add x0, x0, #imm
            base += (insn >> 10) & 0xFFF
        elif insn == 0xD65F03C0 and base is not None:  # ret
            out.append(base)
            base = None
    return out


def profile() -> list[tuple[int, int]]:
    """(capacity %, OCV mV), full -> empty."""
    img = stock.image()
    tables = {stock.read(img, a, ENTRY * MAX_ENTRIES) for a in _tables(img)}
    if len(tables) != 1:
        raise SystemExit(f"temperature-dependent profiles ({len(tables)}): extend the DT table per temperature")
    raw = struct.unpack(f"<{2 * MAX_ENTRIES}i", tables.pop())
    pts, seen = [], set()
    for dod, mv in zip(raw[0::2], raw[1::2]):
        if dod <= 100 and dod not in seen:
            seen.add(dod)
            pts.append((100 - dod, mv))
    return pts


def dt(step: int = 5) -> str:
    pts = [p for p in profile() if p[0] % step == 0]
    cells = " ".join(f"{mv * 1000} {cap}" for cap, mv in pts)
    return f"ocv-capacity-celsius = <25>;\nocv-capacity-table-0 = <{cells}>;"
