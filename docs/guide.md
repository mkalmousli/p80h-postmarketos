# Guide

## 0. Facts
* `adb shell getprop`: Teclast P80h, board `mt8163`, `ro.product.device=p80h_d8c1_g`, Android 5.1, arm64 kernel 3.10.72 with 32-bit userspace.
* Stock boot.img: pagesize 2048, kernel 0x40080000, ramdisk 0x44000000, tags 0x4e000000, cmdline `bootopt=64S3,32N2,64N2`.
  Kernel in the image = 512-byte MTK header + gzip + DTB appended.
* Partitions: boot is 16 MB, system ~1.6 GB, userdata ~800 MB as reported by LK.

## 1. Host tools
`apt install fastboot abootimg kpartx e2fsprogs device-tree-compiler`; pmbootstrap from git (PyPI releases are yanked);
mtkclient in a venv + its udev rules (`Setup/Linux/*.rules`).

## 2. Bootloader unlock
Enable *OEM unlocking* in Developer options, `adb reboot bootloader`, `fastboot flashing unlock`,
confirm on the tablet with **Vol+**. Data is wiped. (`fastboot oem unlock` is not understood; `flashing unlock` is.)

## 3. Backup (do this BEFORE flashing)
`scripts/00-backup.sh`. BROM mode: tablet fully off, screen black, hold Vol-, plug USB straight into the PC (no hub).
If a Chinese boot-mode menu appears you were too late; the USB connection often drops on big reads, so dump small partitions
and skip system/userdata. Full-flash dumps died at ~1%.

## 4. Build
`scripts/01-extract-boot.sh boot.bin` then `scripts/02-build-image.sh`.
* No kernel source exists, so the stock kernel is wrapped in a package (`linux-teclast-p80h`) with the `kernel.release` and an empty modules dir that `mkinitfs` insists on.
* pmbootstrap's own boot.img (raw Image) is 23 MB > 16 MB boot partition; `03-make-boot.sh` uses the stock gzip kernel instead.
* `pmbootstrap install --split` fails here (kpartx), so `04-make-rootfs.sh` runs `mke2fs -d` on the chroot.

## 5. Flash
`fastboot flash boot local/out/boot.img`. The bootloader rejects fastboot's sparse images for userdata (`Unknown chunk type`),
so write the root image raw: `mtk w userdata local/out/root.img` in BROM mode.
