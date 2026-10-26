"""flash-rootfs: write local/out/root-desktop.img onto userdata from the rescue initramfs (nothing mounted).
The USB NCM link resets under long transfers, so the image goes as 8 MiB gzipped chunks served over HTTP from
this PC; the tablet (src/tools/rootfs-flash.sh) fetches, md5-checks and retries each one, and writes all-zero
chunks from /dev/zero. Boot the rescue kernel first: `p80h kernel ... --rescue`, flash it to recovery,
`p80h reboot recovery`."""
import gzip
import hashlib
import threading
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from .channels.daemon import DaemonChannel
from .rootfs import OUT

CHUNKS, CH, HOST, PORT = Path("local/out/chunks"), 8 << 20, "172.16.42.2", 8180


def chunk() -> int:
    CHUNKS.mkdir(parents=True, exist_ok=True)
    for f in CHUNKS.iterdir():
        f.unlink()
    idx, n = [], 0
    with OUT.open("rb") as src:
        while b := src.read(CH):
            if b.count(0) != len(b):
                (CHUNKS / f"{n:04d}.gz").write_bytes(gzip.compress(b, 1))
                idx.append(f"{n} {hashlib.md5(b).hexdigest()}")
            n += 1
    (CHUNKS / "index").write_text("\n".join(idx) + "\n")
    return n


def flash() -> str:
    n = chunk()
    handler = partial(SimpleHTTPRequestHandler, directory=str(CHUNKS.parent))
    srv = ThreadingHTTPServer((HOST, PORT), handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    ch = DaemonChannel()
    if ch.sh("grep -c mmcblk0p23 /proc/mounts") != "0":
        raise SystemExit("userdata is mounted: boot the rescue kernel first")
    script = Path("src/tools/rootfs-flash.sh").read_text().replace("N=311", f"N={n}")
    ch.put("/tmp/flash.sh", script.encode(), 0o755)
    ch.sh("rm -f /tmp/flash.progress; setsid /tmp/flash.sh > /tmp/flash.log 2>&1 < /dev/null &")
    while (p := ch.sh("cat /tmp/flash.progress 2>/dev/null")) != "done":
        print(p or "starting", end="\r", flush=True)
        time.sleep(10)
    srv.shutdown()
    return f"userdata <- {OUT} ({n} chunks)"
