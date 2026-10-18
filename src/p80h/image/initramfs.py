"""Unpack / patch / repack a pmOS initramfs (zstd or gzip cpio). The 3.10 kernel only understands gzip."""
import gzip
import subprocess
from pathlib import Path


def _decompress(blob: bytes) -> bytes:
    if blob[:4] == bytes.fromhex("28b52ffd"):
        return subprocess.run(["zstd", "-dc"], input=blob, capture_output=True, check=True).stdout
    return gzip.decompress(blob)


def unpack(path: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    # device nodes cannot be created unprivileged; the initramfs makes them itself via devtmpfs
    subprocess.run(["cpio", "-id", "--quiet", "--no-absolute-filenames"], input=_decompress(path.read_bytes()),
                   cwd=dest, capture_output=True)


def pack(src: Path) -> bytes:
    names = subprocess.run(["find", "."], cwd=src, capture_output=True, check=True).stdout
    cpio = subprocess.run(["cpio", "-o", "-H", "newc", "--quiet"], input=names, cwd=src, capture_output=True, check=True)
    return gzip.compress(cpio.stdout, 9)


def patch_init(src: Path, after: str, insert: str) -> None:
    """Insert shell lines right after the first line of init_2nd.sh that starts with `after`."""
    f = src / "init_2nd.sh"
    lines = f.read_text().split("\n")
    i = next(n for n, line in enumerate(lines) if line.startswith(after))
    f.write_text("\n".join(lines[: i + 1] + insert.split("\n") + lines[i + 1:]))
