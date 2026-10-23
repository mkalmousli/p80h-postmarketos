"""adb channel: stock Android or any system running adbd, on the tablet only (other phones on adb are ignored)."""
import subprocess


def _serial() -> str | None:
    out = subprocess.run(["adb", "devices"], capture_output=True, text=True).stdout
    for line in out.splitlines()[1:]:
        serial, _, state = line.partition("\t")
        if state != "device":
            continue
        dev = subprocess.run(["adb", "-s", serial, "shell", "getprop", "ro.product.device"],
                             capture_output=True, text=True, timeout=10).stdout
        if "p80h" in dev:
            return serial
    return None


class AdbChannel:
    name = "adb"

    def available(self) -> bool:
        return _serial() is not None

    def sh(self, cmd: str, timeout: float = 30) -> str:
        r = subprocess.run(["adb", "-s", _serial() or "", "shell", cmd], capture_output=True, text=True, timeout=timeout)
        return r.stdout + r.stderr
