# Releasing (public downloads)

Final results are published as GitHub Releases: <https://github.com/mkalmousli/p80h-postmarketos/releases>.
Every asset has a line in `SHA256SUMS`. Flashing can brick the tablet: **back up first** (`local/dumps/` method in `guide.md`).

## What is published
Only things we may redistribute: mainline kernel build, our dts/patches, initramfs/boot.img built from them, the `p80hd` daemon, the postmarketOS rootfs.

## What is never published
- Anything under `local/dumps/` (nvram, proinfo, persist, protect, seccfg, secro hold MAC/serial/IMEI/calibration). `p80h release` refuses these paths.
- Stock Teclast/MediaTek kernel, ramdisk, LK, and firmware blobs (`mt6627`, `WMT_SOC.cfg`, `WIFI_RAM_CODE_8163`, `md32_*`): proprietary, no redistribution licence. The installer extracts them from the user's own tablet.

## How
```
uv run p80h release v0.1 local/out/boot.img local/out/root.img     # stage + checksums only
uv run p80h release v0.1 local/out/boot.img local/out/root.img --publish
```
