"""adb channel: stock Android or any system running adbd."""
import subprocess


class AdbChannel:
    name = "adb"

    def available(self) -> bool:
        out = subprocess.run(["adb", "devices"], capture_output=True, text=True).stdout
        return "\tdevice" in out

    def sh(self, cmd: str, timeout: float = 30) -> str:
        r = subprocess.run(["adb", "shell", cmd], capture_output=True, text=True, timeout=timeout)
        return r.stdout + r.stderr
