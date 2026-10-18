"""Tablet: one handle that works in every boot stage (BROM/preloader, fastboot, Android/pmOS shell)."""
import time
from .ops import DaemonOps

from . import brom, fastboot, usb
from .channels import ALL

NATIVE_MODES = ("recovery", "bootloader")


class Tablet(DaemonOps):
    def __init__(self) -> None:
        self._channels = [c() for c in ALL]

    def stage(self) -> str:
        dev = usb.find()
        return dev.stage if dev else "off"

    def channel(self):
        """The first shell channel that answers, or None."""
        if self.stage() != "gadget":
            return None
        return next((c for c in self._channels if c.available()), None)

    def sh(self, cmd: str, timeout: float = 30) -> str:
        ch = self.channel()
        if not ch:
            raise RuntimeError(f"no shell channel (stage: {self.stage()})")
        return ch.sh(cmd, timeout)

    def status(self) -> dict:
        stage, ch = self.stage(), self.channel()
        info = {"stage": stage, "channel": ch.name if ch else None}
        if ch:
            if ch.name == "daemon":
                info.update(ch.status())
            info["os"] = self.sh("grep -h ^PRETTY /etc/os-release 2>/dev/null; getprop ro.build.version.release 2>/dev/null").replace("\n", " ")
            info["kernel"] = self.sh("uname -r")
        return info

    def wait(self, want: str, secs: float = 300) -> bool:
        """Block until the stage (or channel name 'adb'/'net') equals `want`."""
        end = time.time() + secs
        while time.time() < end:
            ch = self.channel()
            if want in (self.stage(), ch.name if ch else None):
                return True
            time.sleep(0.5)
        return False

    def reboot(self, mode: str = "normal") -> None:
        """mode: normal | recovery | bootloader | brom | off"""
        stage = self.stage()
        if stage in ("brom", "preloader"):
            return brom.reset()
        if stage == "fastboot":
            return print(fastboot.reboot())
        if mode == "off":
            return self._try("poweroff")
        ch = self.channel()
        if ch and ch.name == "daemon" and mode != "brom":
            return ch.reboot({"normal": "", "off": "off"}.get(mode, mode))
        if mode in NATIVE_MODES:
            return self._try(f"rebootto {mode} || reboot {mode} || reboot -f")
        self._try("reboot -f")
        if mode == "brom":
            brom.run("gettargetconfig")  # grabbing the preloader parks it there

    def _try(self, cmd: str) -> None:
        try:
            self.sh(f"({cmd}) >/dev/null 2>&1 &", timeout=5)
        except Exception:
            pass  # the link drops as the tablet goes down
