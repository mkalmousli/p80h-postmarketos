# TODO — everything needed for a fully working Alpine/postmarketOS on the P80h

Legend: ✅ done · 🔧 in progress · ⬜ todo · ❓ chip not yet identified (identify first: stock `/sys`, `/proc/iomem`, `re/dt/stock.dts`, PCB photos in `local/`).
"Done" for a driver = works after cold boot, survives suspend/resume, no kernel warnings, documented in `hardware.md`, patch upstreamable.
Order follows dependency: each phase unlocks the next.

## Phase 0 — Access & safety
- ✅ BROM access, full stock backup (`local/dumps/emmc-partitions`, SHA256SUMS)
- ✅ Mainline 6.12.99 boots to initramfs; USB-NCM + `p80hd` reachable
- ⬜ Verify the stock backup restores (flash-back test of every partition on a spare run) and write `docs/unbrick.md`
- ⬜ UART pads located + photographed (`docs/hw`), serial console as a second log channel
- ⬜ Reliable test loop without manual power-cycles (watchdog fallback proven for every failure mode)

## Phase 1 — Storage (current blocker)
- 🔧 **eMMC/MSDC0**: CMD2 gets no response (CMD0/1 fine). Next: compare pad/driver-strength & clock-phase regs with stock (`local/trees/kernel_douglas`), check IOCON/PAD_TUNE, voltage rail (vemc) sequencing, 8-bit DDR/HS200/HS400 tuning
- ⬜ Rootfs on `userdata`, mount by label, `switch_root` to Alpine
- ⬜ Partition layout for Alpine (keep stock partitions restorable); resize/ext4/f2fs
- ⬜ SD card (MSDC1) + hotplug detect, SDIO bus for Wi-Fi if used
- ⬜ eMMC reliability: trim, write-protect of boot0/1, never touch `nvram/proinfo/persist`

## Phase 2 — Core SoC bring-up
- ⬜ Clocks/PLLs complete (mt8163 apmixedsys/topckgen/infracfg/pericfg) — remove the "skip PLLs already on" workaround
- ⬜ Interrupts, timers (CPUXGPT ✅ patched), watchdog (WDT), reboot/poweroff reasons to LK
- ⬜ pinctrl + GPIO complete, EINT
- ⬜ SMP: all 4 Cortex-A53 up via PSCI/SPM (verify `cpu_on/off`), CPU idle
- ⬜ DVFS: cpufreq + regulators, thermal sensor (AUXADC/thermal) and trips
- ⬜ Power domains (SCPSYS), SPM/sleep states
- ⬜ DMA (APDMA), SPI, I2C ×N, UART, PWM, AUXADC, RTC, efuse/serial number
- ⬜ Mainline DTS: `mt8163-teclast-p80h.dts` complete and cleaned, no vendor hacks; patch series upstreamed

## Phase 3 — Power & PMIC
- ⬜ **PMIC (MT6323 via pwrap)** probe — currently stalls and is disabled; regulators, RTC, keys, ADC, LDO/buck consumers
- ⬜ Battery fuel gauge + charger (bq24296?): charging, % and health, USB-OTG power, charge-while-off
- ⬜ Power key / volume keys (mtk-kpd), long-press power-off
- ⬜ Suspend-to-RAM and resume with wakeups (power key, USB, RTC); runtime PM
- ⬜ Thermal/charge limits, low-battery shutdown, poweroff leaves charger animation sane

## Phase 4 — USB
- ✅ MUSB peripheral (PIO) + NCM gadget works
- ⬜ MUSB **DMA** (currently `MUSB_PIO_ONLY`) for speed
- ⬜ USB host mode + OTG role switch (keyboards, drives, hubs), VBUS control
- ⬜ Proper PHY driver (replace the register-sequence hack), clean runtime PM
- ⬜ Gadget profiles: NCM/ACM/mass-storage/ADB-like debug, configfs

## Phase 5 — Display & input
- ⬜ Display pipeline (mmsys/ovl/rdma/dsi) + panel `c805_zs_hx8394a01_inx_wxga_ips_8` (HX8394 over DSI) via DRM/KMS; simplefb as interim
- ⬜ Backlight (PWM/PMIC) with brightness control, panel power sequencing, blank/unblank
- ⬜ Touchscreen: Silead **GSL** controller (I2C) — multitouch, firmware load, wake-on-touch
- ⬜ Accelerometer (MIR3DA) → auto-rotate (iio + `iio-sensor-proxy`), correct mount matrix
- ⬜ Other sensors ❓ (ALS/proximity, magnetometer, gyro): identify, then driver or document "absent"
- ⬜ Console on screen (fbcon/VT) and a graphical stack: Wayland (sway/phosh/weston) or X11 with GPU/fbdev

## Phase 6 — GPU & video
- ⬜ GPU: Mali-T720 via panfrost (check mainline support for T720) — GLES, Vulkan not required
- ⬜ Hardware video decode/encode (VDEC/VENC) via V4L2 stateless if feasible; software fallback documented
- ⬜ Display scaling/rotation via GPU/DRM planes

## Phase 7 — Wireless (MediaTek combo, CONSYS/MT6627-class)
- ⬜ CONSYS/WMT stack: firmware (`ROMv2_lm_patch`, `WMT_SOC.cfg`, `WIFI_RAM_CODE_8163`, `mt6627/`) — extraction script (not redistributed)
- ⬜ Wi-Fi 2.4 GHz (+5 GHz if present): scan/connect/AP mode, WPA2/3, power save, MAC from NVRAM
- ⬜ Bluetooth: HCI over WMT, audio (A2DP) + HID
- ⬜ FM radio if present ❓, GPS ❓
- ⬜ Rfkill, airplane mode, regulatory domain

## Phase 8 — Audio
- ⬜ Audio codec/AFE ❓ (MT6323 codec + MT8163 AFE): ASoC machine driver, ALSA UCM profile
- ⬜ Speaker, headphone jack + detect (ACCDET), microphone, volume keys mapping
- ⬜ HDMI/MHL ❓ (verify the hardware exists), BT audio routing
- ⬜ PipeWire/WirePlumber config in the Alpine image

## Phase 9 — Camera & misc
- ⬜ Identify front/rear sensors ❓ and ISP path; V4L2/libcamera driver, flash/LED
- ⬜ Vibrator ❓, LEDs, hall sensor ❓, SIM/modem ❓ (`gemini_support=false` → likely Wi-Fi only; confirm no modem)
- ⬜ NVRAM/`proinfo` readers: serial, Wi-Fi/BT MAC, calibration — read-only, never published

## Phase 10 — Alpine/postmarketOS system
- ⬜ `device-teclast-p80h`, `linux-teclast-p80h`, `firmware-teclast-p80h` APKBUILDs in `src/port/pmaports`, build via `pmbootstrap`, pass `pmbootstrap checks`
- ⬜ Boot flow: LK → boot.img (mainline Image+dtb+initramfs) → OpenRC → Alpine; recovery/fallback entry kept
- ⬜ `p80h install`: one command (backup → verify → flash → rootfs → firmware extract → verify), idempotent, refuses to touch identity partitions
- ⬜ UI: choose and tune (Phosh / Plasma Mobile / Sxmo / Xfce) for 800×1280, on-screen keyboard, rotation, scaling
- ⬜ Networking (NetworkManager/iwd), SSH, time sync, zram, OTA updates (`apk upgrade`), encrypted rootfs option
- ⬜ Performance & battery targets measured (boot time, idle drain, suspend drain)

## Phase 11 — Quality, docs, upstream
- ⬜ Test matrix per subsystem in `tests/` + a boot smoke test script; log every run in `status.md`
- ⬜ `hardware.md` fully filled: every chip, bus, address, GPIO, regulator, with evidence path
- ⬜ Clean-up: remove debug markers/initcall_debug, files ≤100 LOC, CREDITS.md complete
- ⬜ Upstream: send DT + driver patches to linux-mediatek / LKML; submit device package to postmarketOS pmaports
- ⬜ First public release (`p80h release`), then publish bring-up write-up
