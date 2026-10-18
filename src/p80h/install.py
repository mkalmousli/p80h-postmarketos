"""install: make a wrapped mainline kernel the default boot image; the stock boot image moves to recovery as the fallback."""
from pathlib import Path

from .channels.daemon import DaemonChannel

STOCK_BOOT = Path("local/dumps/emmc-partitions/boot.bin")
PART_MAX = 16 * 1024 * 1024


def node_of(ch: DaemonChannel, name: str) -> str:
    ch.sh("mdev -s 2>/dev/null; true")
    return "/dev/" + ch.sh(f"grep -l PARTNAME={name}$ /sys/class/block/mmcblk0p*/uevent").split("/")[4]


def install(image: Path) -> str:
    """Flash `image` to boot and the stock boot image to recovery (key combo power+vol-up reaches it); verified by readback."""
    ch = DaemonChannel()
    plan = ((node_of(ch, "recovery"), STOCK_BOOT), (node_of(ch, "boot"), image))  # fallback first: never leave no way back
    for node, src in plan:
        data = src.read_bytes()
        if not 0 < len(data) <= PART_MAX:
            raise SystemExit(f"{src}: {len(data)} bytes does not fit {node}")
        ch.put(node, data)
        if ch.get(node, 0, len(data)) != data:
            raise SystemExit(f"readback of {node} differs")
    return "installed: " + ", ".join(f"{n} <- {s.name}" for n, s in plan)
