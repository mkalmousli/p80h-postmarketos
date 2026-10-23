"""items: the ITEMS partition, the board config the stock kernel reads at runtime (items_*_get_* calls), as key -> value."""
from pathlib import Path

ITEMS = Path("local/dumps/emmc-partitions/ITEMS.bin")


def items() -> dict[str, str]:
    text = ITEMS.read_bytes().split(b"\0")[0].decode(errors="replace")
    body = text.split("items.start", 1)[1].split("items.end", 1)[0]
    out = {}
    for line in body.splitlines():
        line = line.split("#", 1)[0].strip() if not line.lstrip().startswith("#") else ""
        if line:
            key, _, value = line.partition(" ") if " " in line else line.partition("\t")
            out[key.strip()] = value.strip()
    return out
