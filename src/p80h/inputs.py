"""Input injection: write raw evdev events into the tablet's input devices (works on any Linux with a shell)."""
import struct

EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
KEYS = {"power": 116, "volup": 115, "voldown": 114, "home": 102, "back": 158, "menu": 139, "enter": 28}


def ev(kind: int, code: int, value: int) -> bytes:
    return struct.pack("<QQHHi", 0, 0, kind, code, value)  # timeval is zero: kernel stamps it


def printf_arg(data: bytes) -> str:
    return "".join(f"\\{b:03o}" for b in data)


def key_events(code: int) -> bytes:
    return b"".join([ev(EV_KEY, code, 1), ev(EV_SYN, 0, 0), ev(EV_KEY, code, 0), ev(EV_SYN, 0, 0)])


def touch_events(points: list[tuple[int, int]]) -> bytes:
    """Multitouch type A: each point is X, Y, SYN_MT_REPORT; one SYN_REPORT per sample; empty frame = release."""
    out = b""
    for x, y in points:
        out += ev(EV_ABS, 53, x) + ev(EV_ABS, 54, y) + ev(EV_SYN, 2, 0) + ev(EV_SYN, 0, 0)
    return out + ev(EV_SYN, 2, 0) + ev(EV_SYN, 0, 0)


def find_node(sh, name_re: str) -> str:
    """Return /dev/input/eventN whose sysfs name matches name_re."""
    cmd = f"for d in /sys/class/input/event*; do grep -qiE '{name_re}' $d/device/name && echo /dev/input/$(basename $d) && break; done"
    return sh(cmd).strip()


def send(sh, node: str, data: bytes) -> None:
    sh(f"printf '{printf_arg(data)}' > {node}")
