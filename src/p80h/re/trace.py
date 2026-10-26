"""trace: hardware sequence of a stock kernel function, following its calls down to the register-level leaves.

Leaves (logged, not followed) are the vendor helpers that touch hardware or time: Ana_Set_Reg (MT6323 via
pwrap), Afe_Set_Reg (AFE MMIO), clock/GPIO/regulator calls, delays. Everything else is executed.
"""
from . import stock
from .emu import Emu

LEAVES = ("Ana_Set_Reg", "Afe_Set_Reg", "Ana_Get_Reg", "Afe_Get_Reg", "msleep", "udelay", "__const_udelay", "mdelay",
          "usleep_range", "mt_set_gpio", "enable_clock", "disable_clock", "clk_", "hwPower", "pmic_", "upmu_",
          "pwrap_", "printk", "mutex_", "_raw_spin", "spin_", "AudDrv_", "mt_get_gpio", "regulator_")


def names() -> dict[int, str]:
    out = {}
    for line in stock.OUT.joinpath("symbols.txt").read_text().splitlines():
        a, _, n = line.split()
        out.setdefault(int(a, 16), n)
    return out


def trace(fn: str, args=(), keep=LEAVES, follow=(), mem=None) -> list[tuple]:
    """keep: name prefixes that are leaves; follow: leaf prefixes to descend into anyway (e.g. AudDrv_Clk_On).
    mem: {address: bytes} written before the run, e.g. a zeroed driver-state struct and the global pointing at it."""
    syms, inv = stock.symbols(), names()

    def descend(t: int) -> bool:
        n = inv.get(t, "")
        if any(n.startswith(f) for f in follow):
            return True
        return bool(n) and not any(n.startswith(k) for k in keep)

    e = Emu(syms[fn], 0x800, args=args, descend=descend)
    for addr, data in (mem or {}).items():
        for i, b in enumerate(data):
            e.store(addr + i, 1, b)
    try:
        e.run(200000)
    except Exception as ex:  # unsupported instruction: return what was gathered, marked
        e.log.append(("stop", str(ex), ()))
    out = []
    for kind, t, a in e.log:
        if kind == "stop":
            out.append(("STOP", t))
            continue
        n = inv.get(t, hex(t)) if isinstance(t, int) else str(t)
        if n.startswith(("printk", "mutex_", "_raw_spin", "spin_")):
            continue
        out.append((n,) + tuple(hex(x) if isinstance(x, int) else "?" for x in a[:3]))
    return out
