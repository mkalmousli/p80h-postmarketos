"""Read the tablet's screen with an Android phone on adb pointed at it: Open Camera takes a photo, tesseract reads it."""
import re
import subprocess
import tempfile
import threading
import time
from pathlib import Path

from PIL import Image, ImageOps

PHOTOS = "/sdcard/DCIM/OpenCamera"
_shutter: tuple[int, int] | None = None


def adb(*args: str) -> str:
    return subprocess.run(["adb", *args], capture_output=True, text=True, timeout=20).stdout


def _find_shutter() -> tuple[int, int]:
    global _shutter
    if _shutter is None:
        adb("shell", "am", "start", "-n", "net.sourceforge.opencamera/.MainActivity")
        time.sleep(1.5)
        adb("shell", "uiautomator", "dump", "/sdcard/ui.xml")
        xml = adb("shell", "cat", "/sdcard/ui.xml")
        m = re.search(r'content-desc="Take Photo"[^>]*bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"', xml)
        if not m:
            raise RuntimeError("Open Camera shutter not found on the phone (unlock it first)")
        x1, y1, x2, y2 = map(int, m.groups())
        _shutter = ((x1 + x2) // 2, (y1 + y2) // 2)
    return _shutter


def _newest() -> str:
    return adb("shell", "ls", "-t", PHOTOS).split()[0]


def ocr(photo: Path) -> str:
    """The phone sits portrait but Open Camera saves landscape: rotate 270 deg, boost contrast, read."""
    img = ImageOps.autocontrast(ImageOps.grayscale(Image.open(photo)))
    img.thumbnail((2000, 2000))
    png = photo.with_suffix(".png")
    img.rotate(270, expand=True).save(png)
    out = subprocess.run(["tesseract", str(png), "-", "--psm", "6"], capture_output=True, text=True)
    return "\n".join(l for l in out.stdout.splitlines() if l.strip())


def read_screen() -> str:
    """One photo -> text (blocks about 3 s)."""
    before = _newest()
    x, y = _find_shutter()
    adb("shell", "input", "tap", str(x), str(y))
    for _ in range(10):
        time.sleep(0.4)
        if _newest() != before:
            break
    with tempfile.TemporaryDirectory() as d:
        photo = Path(d) / "shot.jpg"
        adb("pull", f"{PHOTOS}/{_newest()}", str(photo))
        return ocr(photo)


def snap(out: Path, delay: float) -> threading.Thread:
    """Background: `delay` seconds from now take one photo of the (by then static) screen and write its text to `out`."""
    def run():
        time.sleep(delay)
        out.write_text(read_screen() + "\n")
    t = threading.Thread(target=run, daemon=True)
    t.start()
    return t
