"""LK fastboot access. LK here rejects sparse images and `erase`, so only small raw flashes are expected."""
import subprocess


def run(*args: str, timeout: float = 120) -> str:
    r = subprocess.run(["fastboot", *args], capture_output=True, text=True, timeout=timeout)
    return r.stdout + r.stderr


def flash(part: str, path: str) -> str:
    return run("flash", part, path)


def reboot() -> str:
    return run("reboot")
