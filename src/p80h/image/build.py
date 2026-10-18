"""build-boot: initramfs from pmbootstrap -> boot.img containing p80hd (and optionally a no-mount rescue mode)."""
import tempfile
from pathlib import Path

from . import agent, bootimg, initramfs


def build_boot(init: Path, out: Path, stock: Path, rescue: bool = False, debug: bool = False) -> Path:
    with tempfile.TemporaryDirectory() as d:
        root = Path(d, "rd")
        initramfs.unpack(init, root)
        agent.install(root, agent.compile_agent(Path(d, "p80hd")), rescue)
        bootimg.build(initramfs.pack(root), out, stock, "pmos.debug-shell" if debug else "")
    return out
