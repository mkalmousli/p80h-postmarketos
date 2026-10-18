"""Command line: p80h status | sh CMD | reboot [MODE] | wait X | key NAME | tap X Y | flash PART FILE"""
import argparse
import json
import sys
from pathlib import Path

from . import brom, fastboot
from .tablet import Tablet


def main(argv: list[str] | None = None) -> None:
    p = argparse.ArgumentParser(prog="p80h", description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    sub.add_parser("sh").add_argument("command", nargs="+")
    sub.add_parser("reboot").add_argument("mode", nargs="?", default="normal",
                                          choices=["normal", "recovery", "bootloader", "brom", "off"])
    w = sub.add_parser("wait")
    w.add_argument("want")
    w.add_argument("secs", nargs="?", type=float, default=300)
    sub.add_parser("key").add_argument("name")
    t = sub.add_parser("tap")
    t.add_argument("x", type=int)
    t.add_argument("y", type=int)
    f = sub.add_parser("flash")
    f.add_argument("pairs", nargs="+", metavar="PART=FILE")
    f.add_argument("--via", choices=["agent", "brom", "fastboot"], default="agent")
    ps = sub.add_parser("push", help="copy a directory tree onto the tablet")
    ps.add_argument("src")
    ps.add_argument("dest", nargs="?", default="/")
    sub.add_parser("shell", help="interactive terminal")
    sub.add_parser("screenshot").add_argument("out")
    b = sub.add_parser("build-boot", help="pmOS initramfs + p80hd -> MTK boot.img")
    b.add_argument("initramfs")
    b.add_argument("out")
    b.add_argument("--stock", default="port/stock", help="dir with bootimg.cfg + zImage.stock")
    b.add_argument("--rescue", action="store_true", help="park in the initramfs, never mount the rootfs")
    a = p.parse_args(argv)
    if a.cmd == "build-boot":
        from .image.build import build_boot
        return print(build_boot(Path(a.initramfs), Path(a.out), Path(a.stock), a.rescue))
    tab = Tablet()
    if a.cmd == "status":
        print(json.dumps(tab.status()))
    elif a.cmd == "sh":
        print(tab.sh(" ".join(a.command)))
    elif a.cmd == "reboot":
        tab.reboot(a.mode)
    elif a.cmd == "wait":
        sys.exit(0 if tab.wait(a.want, a.secs) else 1)
    elif a.cmd == "push":
        print(tab.push(Path(a.src), a.dest), "files")
    elif a.cmd == "shell":
        from .shell import interactive
        interactive()
    elif a.cmd == "screenshot":
        Path(a.out).write_bytes(tab.screenshot())
    elif a.cmd == "key":
        tab.key(a.name)
    elif a.cmd == "tap":
        tab.tap(a.x, a.y)
    elif a.cmd == "flash":
        parts = dict(x.split("=", 1) for x in a.pairs)
        if a.via == "agent":
            for k, v in parts.items():
                tab.flash(k, Path(v).read_bytes())
        elif a.via == "fastboot":
            print(*(fastboot.flash(k, v) for k, v in parts.items()))
        else:
            brom.write(parts)
            brom.reset()
