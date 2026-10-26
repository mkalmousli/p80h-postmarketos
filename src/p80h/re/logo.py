"""logo: the MTK `logo` partition (what LK draws: boot logo, charging animation, digits) <-> a folder of PNGs.

Layout: 512-byte MTK header ("LOGO", payload size), then u32 count, u32 total, count u32 offsets, and count zlib
blobs of raw 32-bit pixels (BGRA, 800 wide for the full-screen ones). Small images are not full width; their size
is kept in sizes.txt (w h per image) so `pack` rebuilds the same layout.
"""
import struct
import zlib
from pathlib import Path

from PIL import Image

W = 800


def _shape(n: int, hint: tuple[int, int] | None) -> tuple[int, int]:
    if hint:
        return hint
    px = n // 4
    if px % W == 0:
        return W, px // W
    for w in range(min(px, 800), 0, -1):  # small images: the widest exact fit up to 800
        if px % w == 0 and px // w <= 2 * w:
            return w, px // w
    return px, 1


def unpack(src: Path, out: Path) -> int:
    d = src.read_bytes()
    body = d[512:]
    n, total = struct.unpack_from("<II", body, 0)
    offs = list(struct.unpack_from(f"<{n}I", body, 8)) + [total]
    out.mkdir(parents=True, exist_ok=True)
    sizes = []
    for i in range(n):
        raw = zlib.decompress(body[offs[i]:offs[i + 1]])
        w, h = _shape(len(raw), None)
        Image.frombytes("RGBA", (w, h), raw, "raw", "BGRA").save(out / f"{i:02d}.png")
        sizes.append(f"{w} {h}")
    (out / "sizes.txt").write_text("\n".join(sizes) + "\n")
    return n


def pack(folder: Path, dst: Path, size: int = 8 << 20) -> int:
    sizes = [tuple(map(int, l.split())) for l in (folder / "sizes.txt").read_text().split("\n") if l]
    blobs = []
    for i, (w, h) in enumerate(sizes):
        im = Image.open(folder / f"{i:02d}.png").convert("RGBA").resize((w, h))
        blobs.append(zlib.compress(im.tobytes("raw", "BGRA"), 9))
    n = len(blobs)
    head = 8 + 4 * n
    offs, pos = [], head
    for b in blobs:
        offs.append(pos)
        pos += len(b)
    body = struct.pack(f"<II{n}I", n, pos, *offs) + b"".join(blobs)
    hdr = bytearray(b"\xff" * 512)
    hdr[0:4] = bytes.fromhex("88168858")
    hdr[4:8] = struct.pack("<I", len(body))
    hdr[8:40] = b"LOGO".ljust(32, b"\0")
    img = bytes(hdr) + body
    dst.write_bytes(img + b"\0" * (size - len(img)))
    return len(img)


def art(stock: Path, out: Path) -> Path:
    """The clock's look for every LK screen (src/clock/tools/bootframe.c, rendered by the clock's own code):
    boot logo 0/38 = the clock face with dashes, 1 low battery, 2 charging, 3 charger error, 25-35 charging
    0..100 %, 37 full. LK's small overlays (charge bars, percent digits) are blanked: they are portrait."""
    import shutil
    import subprocess
    import tempfile
    src = Path(__file__).resolve().parents[3] / "src" / "clock"
    tool = Path(tempfile.mkdtemp()) / "bootframe"
    subprocess.run(["gcc", "-O2", "-I", str(src), "-o", str(tool), str(src / "tools/bootframe.c"), str(src / "gfx.c"),
                    "-lm"], check=True)
    folder = Path(tempfile.mkdtemp()) / "logo"
    shutil.copytree(stock, folder)
    jobs = {0: ["boot"], 38: ["boot"], 1: ["low"], 2: ["charge", "15"], 3: ["error"], 37: ["charge", "100"]}
    jobs |= {i: ["charge", str((i - 25) * 10)] for i in range(25, 36)}
    for i, (w, h) in enumerate(tuple(map(int, l.split())) for l in (folder / "sizes.txt").read_text().split("\n") if l):
        if i in jobs:
            raw = subprocess.run([str(tool), *jobs[i]], check=True, capture_output=True).stdout
            im = Image.frombytes("RGBA", (W, 1280), raw, "raw", "BGRA")
            im.putalpha(255)
            im.save(folder / f"{i:02d}.png")
        elif (w, h) != (W, 1280):
            Image.new("RGBA", (w, h), (0, 0, 0, 0)).save(folder / f"{i:02d}.png")
    pack(folder, out)
    return out
