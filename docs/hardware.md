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
| GPU | MediaTek "Sapphire Lite" (`yolig3d`, ID 0x02075017 at 0x13000020), not Mali despite the stock lib names; no open driver, software rendering |
| Firmware files | `/system/etc/firmware`: `ROMv2_lm_patch_1_{0,1}_hdr.bin`, `WIFI_RAM_CODE_8163`, `WMT_SOC.cfg`, `md32_{d,p}.bin`, `mt6627/` |

Raw dumps live outside the repo (device-specific, may contain identifiers).

## Ground truth from the stock binaries (`p80h re i2c`, `p80h re items`)
Board data the stock 3.10 kernel actually uses, taken from the stock `boot.bin` kernel (symbols recovered with vmlinux-to-elf) and the `ITEMS` partition (runtime board config read by `item_integer()` / `items_*_get_cust_acc_hw`).

| Device | Bus / addr | Evidence |
|---|---|---|
| Charger FAN5405 (= bq24156 register map) | I2C0 0x6a | `fan5405_init` board info; live dump in stock dmesg `[fan5405] 00=d0 01=78 02=8e 03=41 04=84 05=03 06=70` |
| Touch Silead GSLx68x (`mtk-tpd`, model C805G_HK) | I2C0 0x40 | ITEMS `touchpanel.ic`, `touchpanel.gsl.modle`; mainline probe (run 78) |
| Accelerometer MIR3DA | I2C2 0x26, orientation 7 | `mir3da_init` board info, ITEMS `accelerometer.i2c = 2`, `accelerometer.mir3da.orientation = 7` |
| Buck SY8827 (`sym827`, likely VPROC) | I2C1 0x60 | `sym827_init` board info |
| Camera sensors (WV511AAF AF, `kd_camera_hw`) | I2C0 0x18 / bus2 | `CAMERA_HW_i2C_init`; ITEMS orientations main 90, sub 270 |
| SD card detect | GPIO28 = EINT6 | ITEMS `sd.dete.pin = 28`, `sd.dete.enit.num = 6`, read in `mt_msdc_init` |
| Panel | `c805_zs_hx8394a01_inx_wxga_ips_8` | ITEMS `LCM` |
| HDMI | present | ITEMS `hdmi.exist = 1` |

Probed-but-absent alternatives compiled into stock (other board variants): gt9xx touch, BMA222 / STK831x / MC3XXX accelerometers.
