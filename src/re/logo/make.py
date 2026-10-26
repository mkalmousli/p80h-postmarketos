#!/usr/bin/env python3
"""make.py STOCK_DIR OUT_DIR: our boot artwork, plain on purpose (black, white, monospace, no glow: cheap to look at and
to drive). Copies the unpacked stock logo folder and redraws every full-screen picture: boot logo (0, 38), low
battery (1), charging (2), charger error (3), charge frames (26-37). The mark is mk.svg rendered 1-bit white."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H = 800, 1280
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono%s.ttf"
WHITE, GREY = 255, 150
HERE = Path(__file__).parent


def font(px: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(MONO % ("-Bold" if bold else ""), px)


def mark(px: int) -> Image.Image:
    with tempfile.TemporaryDirectory() as d:
        png = Path(d, "mk.png")
        subprocess.run(["inkscape", str(HERE / "mk.svg"), "-o", str(png), "-w", str(px), "-h", str(px)], check=True, capture_output=True)
        a = Image.open(png).convert("RGBA").getchannel("A")
    return a.point(lambda v: 255 if v > 100 else 0)  # 1-bit: old-school, no antialiased glow


def canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    im = Image.new("L", (W, H), 0)
    return im, ImageDraw.Draw(im)


def say(d: ImageDraw.ImageDraw, y: int, text: str, px: int = 34, fill: int = WHITE, bold: bool = False) -> None:
    d.text((W // 2, y), text, font=font(px, bold), fill=fill, anchor="mm")


def logo() -> Image.Image:
    im, d = canvas()
    im.paste(255, (W // 2 - 150, 430), mark(300))
    say(d, 840, "p80h", 56, bold=True)
    say(d, 910, "mainline linux", 28, GREY)
    return im


def status(title: str, line: str, pct: int | None = None) -> Image.Image:
    im, d = canvas()
    im.paste(255, (W // 2 - 60, 120), mark(120))
    say(d, 560, title, 44, bold=True)
    if pct is not None:
        d.rectangle((150, 640, 650, 680), outline=WHITE, width=3)
        d.rectangle((154, 644, 154 + int(492 * pct / 100), 676), fill=WHITE)
    say(d, 760, line, 28, GREY)
    return im


def main(stock: Path, out: Path) -> None:
    shutil.copytree(stock, out, dirs_exist_ok=True)
    pics = {0: logo(), 38: logo(), 1: status("battery low", "plug in a charger", 5),
            2: status("charging", "", 50), 3: status("charger error", "unplug the charger")}
    for i in range(26, 35):
        pics[i] = status("charging", "", 5 + (i - 26))
    pics[35] = status("charging", "", 50)
    pics[36] = status("charging", "", 85)
    pics[37] = status("charged", "100%", 100)
    for i, im in pics.items():
        im.convert("RGBA").save(out / f"{i:02d}.png")


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
