# Teclast P80h hardware survey (stock Android 5.1, kernel 3.10.72)

Collected over `adb` from the stock firmware (`alps/full_joyasz8163_s_es`, MT8163 S01).

| Block | Finding |
|---|---|
| SoC | MediaTek MT8163, 4x Cortex-A53 (arm64), `ro.board.platform=mt8163` |
| Kernel | 3.10.72 PREEMPT, built 2017-09-29, **no loadable modules** (`lsmod` empty, no `/system/lib/modules`), no `/proc/config.gz` |
| Display | `mtkfb` fbdev, `fb0` 800x1280, 32 bpp, virtual size 800x3840 (3 buffers), backlight `lcd-backlight` |
| Console | **No VT / fbcon**: no `/sys/class/vtconsole`, no `tty0` -> kernel console never reaches the screen |
| Touch | `mtk-tpd` with `gsl_gesture` (Silead GSL touch controller) |
| Accelerometer | MIR3DA (`m_acc_input`) |
| Keys | `mtk-kpd`, `mtk-tpd-kpd` |
| Wi-Fi / BT | MediaTek combo chip over CONSYS (`mediatek.wlan.chip=CONSYS_MT8163`), firmware dir `mt6627` |
| GPU | Mali (`libGLES_mali.so`, `gralloc.mt8163.mali.so`), vendor blobs only |
| Firmware files | `/system/etc/firmware`: `ROMv2_lm_patch_1_{0,1}_hdr.bin`, `WIFI_RAM_CODE_8163`, `WMT_SOC.cfg`, `md32_{d,p}.bin`, `mt6627/` |

Raw dumps live outside the repo (device-specific, may contain identifiers).
