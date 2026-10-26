"""provision: make the running tablet's rootfs a desktop: apk packages (src/port/packages), the overlay tree
(src/port/overlay, mirrored onto /), services, firmware. Needs internet on the tablet: run
src/tools/share-net.sh on the PC first (usbnet.start routes through it). Idempotent."""
from pathlib import Path

from .channels.daemon import DaemonChannel
from .image.daemon import BOOT_SRC, compile_charge
from .touchfw import OUT as TOUCH_FW

PORT = Path("src/port")
SERVICES = (("udev", "sysinit"), ("udev-trigger", "sysinit"), ("udev-settle", "sysinit"), ("dbus", "default"),
            ("networkmanager", "default"), ("bluetooth", "default"), ("chronyd", "default"),
            ("local", "default"))


def packages() -> tuple[list[str], list[str]]:
    lines = [ln.split("#")[0].strip() for ln in (PORT / "packages").read_text().splitlines()]
    names = [ln for ln in lines if ln]
    return [n for n in names if not n.startswith("?")], [n[1:] for n in names if n.startswith("?")]


def push_overlay(ch: DaemonChannel) -> int:
    root = PORT / "overlay"
    files = sorted(p for p in root.rglob("*") if p.is_file())
    for d in sorted({"/" + str(p.parent.relative_to(root)) for p in files}):
        ch.sh(f"mkdir -p '{d}'")
    for p in files:
        ch.put("/" + str(p.relative_to(root)), p.read_bytes(), p.stat().st_mode & 0o777)
    return len(files)


def provision() -> str:
    ch = DaemonChannel()
    log = [f"overlay: {push_overlay(ch)} files"]
    ch.sh("/etc/local.d/usbnet.start; mkdir -p /etc/p80h /lib/firmware/silead; rm -f /etc/local.d/openbox.start /root/.config/autostart/p80h-audio.desktop")
    ch.put("/usr/local/bin/p80h-splash", compile_charge(Path("local/out/p80h-splash"), BOOT_SRC).read_bytes(), 0o755)  # --bar: progress bar over X
    ch.put("/lib/firmware/silead/gsl1680-teclast-p80h.fw", TOUCH_FW.read_bytes(), 0o644)
    need, optional = packages()
    out, rc = ch.run("apk update -q && apk add -q " + " ".join(need) + " 2>&1 | tail -20", timeout=3600)
    log.append(f"apk add ({len(need)}): rc={rc} {out}")
    for name in optional:
        log.append(f"{name}: " + ch.sh(f"apk add -q {name} >/dev/null 2>&1 && echo ok || echo missing", timeout=600))
    for svc, level in SERVICES:
        ch.sh(f"[ -e /etc/init.d/{svc} ] && rc-update add {svc} {level} >/dev/null 2>&1")
    # postmarketOS ships mdev hotplug in the initramfs; udev owns it on the rootfs (libinput needs udev's db)
    ch.sh("rc-update del mdev sysinit >/dev/null 2>&1; udevadm hwdb --update 2>/dev/null; true")
    log.append(ch.sh("ls -l /etc/local.d/session.start; command -v openbox firefox nmcli bluetoothctl"))
    return "\n".join(log)
