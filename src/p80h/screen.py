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
