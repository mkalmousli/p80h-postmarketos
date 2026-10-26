"""Screenshot: read the mtkfb framebuffer over the daemon and write a PNG (stdlib only)."""
import struct
import zlib

W, H, VH = 800, 1280, 3840  # visible size; the virtual framebuffer is taller (page flipping)


def png(rgb: bytes, w: int, h: int) -> bytes:
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def from_fb(fb: bytes, yoffset: int = 0) -> bytes:
    """32 bpp little-endian BGRA scanlines -> PNG."""
    px = fb[yoffset * W * 4:(yoffset + H) * W * 4]
    rgb = bytearray(W * H * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = px[2::4], px[1::4], px[0::4]
    return png(bytes(rgb), W, H)


OVL0 = 0x14007000  # layer 0 registers (mt8173 layout, see src/re/golden/lk-display.txt)


DEVMEM = "/tmp/dm/devmem"  # src/tools/devmem.c: MMIO needs mmap, read() on /dev/mem only works for RAM


def _regs(ch, base: int, n: int) -> list[int]:
    out = ch.sh("; ".join(f"{DEVMEM} {base + 4 * i:#x}" for i in range(n)))
    return [int(v, 16) for v in out.split()]


def scanout(ch) -> tuple[bytes, dict]:
    """What the panel is fed right now: OVL0 layer 0 address/size/pitch -> that memory -> PNG (any kernel, any fb)."""
    src_con = _regs(ch, OVL0 + 0x2c, 1)[0]
    con, _, size, _, _, pitch = _regs(ch, OVL0 + 0x30, 6)[:6]
    addr = _regs(ch, OVL0 + 0xf40, 1)[0]
    w, h, stride = size & 0xFFFF, size >> 16, pitch & 0xFFFF
    info = {"layer_en": src_con & 1, "addr": hex(addr), "w": w, "h": h, "pitch": stride, "con": hex(con)}
    if not (src_con & 1 and w and h and stride >= 4 * w):
        return b"", info
    chunk = 1 << 18  # one big /dev/mem read crawls (minutes), 256 KiB pieces take ms each
    fb = b"".join(ch.get("/dev/mem", addr + o, min(chunk, stride * h - o)) for o in range(0, stride * h, chunk))
    px = b"".join(fb[y * stride:y * stride + 4 * w] for y in range(h))
    rgb = bytearray(w * h * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = px[2::4], px[1::4], px[0::4]
    return png(bytes(rgb), w, h), info
