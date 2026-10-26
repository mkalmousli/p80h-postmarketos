"""rootfs: the desktop rootfs built on the PC, so a fresh tablet needs no `apk add`: the pmbootstrap rootfs chroot
+ src/port/packages (apk run inside it through the static qemu-aarch64 of the native chroot, binfmt 'F' so nothing
is copied in) + src/port/overlay + services + touch firmware -> ext4 image labelled pmOS_root. The filesystem is
grown to the partition on first boot (resize.start). Needs sudo (chroot, ownership)."""
import shutil
import subprocess
from pathlib import Path

from .provision import PORT, SERVICES, packages
from .touchfw import OUT as TOUCH_FW

PMB = Path("local/build/pmos-work")
BASE, WORK, OUT = PMB / "chroot_rootfs_teclast-p80h", Path("local/build/rootfs-desktop"), Path("local/out/root-desktop.img")
QEMU = (PMB / "chroot_native/usr/bin/qemu-aarch64").resolve()
CACHE = (PMB / "cache_apk_aarch64").resolve()
REPOS = " ".join(f"https://dl-cdn.alpinelinux.org/alpine/edge/{r}" for r in ("main", "community", "testing"))
BINFMT = (r":qemu-aarch64:M::\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7\x00:"
          r"\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:" + f"{QEMU}:F")


def sudo(*cmd, check=True, **kw):
    return subprocess.run(["sudo", *map(str, cmd)], check=check, **kw)


def chroot(cmd: str, check=True):
    return sudo("chroot", WORK, "/bin/sh", "-c", cmd, check=check)


def build(fresh: bool = False) -> Path:
    if not Path("/proc/sys/fs/binfmt_misc/qemu-aarch64").exists():
        sudo("sh", "-c", f"printf '%s' '{BINFMT}' > /proc/sys/fs/binfmt_misc/register")
    if fresh or not WORK.exists():
        sudo("rm", "-rf", WORK)
        sudo("cp", "-a", BASE, WORK)
    mounts = [("/proc", "proc"), ("/dev", "dev"), ("/sys", "sys"), (CACHE, "var/cache/apk")]
    for src, dst in mounts:
        sudo("mkdir", "-p", WORK / dst)
        sudo("mount", "--bind", src, WORK / dst)
    try:
        sudo("cp", "/etc/resolv.conf", WORK / "etc/resolv.conf")
        sudo("mkdir", "-p", WORK / "etc/apk/keys")  # pmbootstrap keeps keys and repos outside the chroot
        sudo("sh", "-c", f"cp {PMB}/config_apk_keys/* {WORK}/etc/apk/keys/ && printf '%s\\n' {REPOS} > {WORK}/etc/apk/repositories")
        need, optional = packages()
        chroot("apk update -q && apk add -q --no-progress " + " ".join(need))
        for name in optional:
            chroot(f"apk add -q --no-progress {name} || echo 'skipped {name}'", check=False)
        sudo("cp", "-r", "--no-preserve=ownership", f"{PORT / 'overlay'}/.", f"{WORK}/")
        sudo("mkdir", "-p", WORK / "lib/firmware/silead")
        sudo("cp", TOUCH_FW, WORK / "lib/firmware/silead/gsl1680-teclast-p80h.fw")
        for svc, level in SERVICES:
            chroot(f"[ -e /etc/init.d/{svc} ] && rc-update add {svc} {level} >/dev/null; true")
        chroot("rc-update del mdev sysinit >/dev/null 2>&1; udevadm hwdb --update 2>/dev/null; "
               "rm -f /etc/local.d/openbox.start; true")
    finally:
        for _, dst in reversed(mounts):
            sudo("umount", "-l", WORK / dst, check=False)
    used = int(subprocess.run(["sudo", "du", "-sm", WORK], capture_output=True, text=True).stdout.split()[0])
    OUT.unlink(missing_ok=True)
    sudo("mke2fs", "-q", "-t", "ext4", "-L", "pmOS_root", "-d", WORK, OUT, f"{used * 13 // 10 + 400}M")
    sudo("chown", f"{shutil.os.getuid()}:{shutil.os.getgid()}", OUT)
    return OUT
