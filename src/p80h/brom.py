"""MediaTek preloader/BROM access through mtkclient. Any reboot passes the preloader, so we simply keep retrying."""
import os
import shutil
import subprocess
import time

from . import usb


def _cmd() -> list[str]:
    exe = os.environ.get("P80H_MTK") or shutil.which("mtk")
    if not exe:
        raise SystemExit("mtkclient not found: install it (uv tool install mtkclient) or set P80H_MTK")
    return ["sudo", *exe.split()]


def run(*args: str, ok: str = "", secs: float = 600) -> str:
    """Run an mtk subcommand, retrying until `ok` shows up in its output (the preloader window is short)."""
    end = time.time() + secs
    while time.time() < end:
        r = subprocess.run(_cmd() + list(args), capture_output=True, text=True)
        out = r.stdout + r.stderr
        if r.returncode == 0 and ok in out:
            return out
        time.sleep(0.5)
    raise TimeoutError(f"mtk {' '.join(args)}: preloader not caught in {secs:.0f}s")


def write(parts: dict[str, str], secs: float = 600) -> None:
    run("w", ",".join(parts), ",".join(parts.values()), ok="Wrote", secs=secs)


def reset() -> None:
    run("reset", secs=60)


def present() -> bool:
    dev = usb.find()
    return bool(dev and dev.stage in ("brom", "preloader"))
