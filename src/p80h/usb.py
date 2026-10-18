"""Enumerate USB devices from sysfs and classify the tablet by vendor:product id."""
from dataclasses import dataclass
from pathlib import Path

SYSFS = Path("/sys/bus/usb/devices")

# (vid, pid) -> boot stage; 18d1:d001 is shared by the Android and pmOS gadgets, resolved by probing channels
STAGES = {
    ("0e8d", "0003"): "brom",
    ("0e8d", "2000"): "preloader",
    ("0e8d", "2001"): "preloader",
    ("0bb4", "0c01"): "fastboot",
    ("18d1", "d00d"): "fastboot",
    ("18d1", "d001"): "gadget",
    ("0e8d", "201d"): "gadget",
    ("0bb4", "0c02"): "gadget",
}


@dataclass(frozen=True)
class Device:
    vid: str
    pid: str
    stage: str
    path: str


def _read(dev: Path, name: str) -> str:
    try:
        return (dev / name).read_text().strip()
    except OSError:
        return ""


def find() -> Device | None:
    """Return the tablet if any known id is present on the bus."""
    for dev in sorted(SYSFS.iterdir()):
        vid, pid = _read(dev, "idVendor"), _read(dev, "idProduct")
        stage = STAGES.get((vid, pid))
        if stage:
            return Device(vid, pid, stage, dev.name)
    return None
