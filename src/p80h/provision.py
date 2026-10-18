"""provision: push the X session (fbdev Xorg + openbox + a terminal) onto the running tablet's rootfs."""
from pathlib import Path

from .channels.daemon import DaemonChannel
from .touchfw import OUT as TOUCH_FW

ROOTFS = Path("src/port/rootfs")
FILES = (("xorg.conf", "/etc/X11/xorg.conf", 0o644), ("openbox.start", "/etc/local.d/openbox.start", 0o755),
         ("autostart", "/root/.config/openbox/autostart", 0o644))


def provision() -> str:
    ch = DaemonChannel()
    ch.sh("mkdir -p /root/.config/openbox; rc-update add local default 2>&1 | tail -1")
    for name, dest, mode in FILES:
        ch.put(dest, (ROOTFS / name).read_bytes(), mode)
    ch.sh("mkdir -p /lib/firmware/silead")
    ch.put("/lib/firmware/silead/gsl1680-teclast-p80h.fw", TOUCH_FW.read_bytes(), 0o644)
    return ch.sh("ls -l /etc/local.d/openbox.start")
