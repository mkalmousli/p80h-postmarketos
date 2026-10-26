# Status / bring-up log

Hypothesis -> test -> result. One change per run.

- Critical PLL/mux prepared while running stalls the bus -> ARMPLL removed, mux CRITICAL dropped -> boots to initramfs.
- Preparing any PLL LK already runs (univpll) stalls -> apmixed prepare skips PLLs that are already on -> USB clocks enable fine.
- PHY register read at 0x11210800+0x6b works; writes under test (kernel-20 logs every write).
- Open: pwrap/PMIC probe stalls (disabled), mmc0 untested.

## Run 26/27 (CPUXGPT enable + 'M'-fill probe)
- Added `timer-mt8163-cpuxgpt.c` (patch 0006): sets EN_CPUXGPT via mcucfg 0x670/0x674. Result unknown: tablet fell back to stock after ~77 s.
- `/proc/last_kmsg` after fallback still held the *stock* pre-flash log (ends "Restarting system with command 'recovery'"), no 'M' fill, no "Linux version": mainline never wrote 0x44400000, or never reached the ramlog initcall.
- Next: prove ramlog writes (e.g. fill from an early_initcall / earlycon-style boot console), then eMMC raw log once mmc works.

## Run 28/29 (RAM console address)
- Stock dmesg: ram_console va ffffff800000c000, size 0x10000; `dormant init ... va:ffffff800000c300 pa:43f00300` => PA is **0x43f00000**, not 0x44400000 (that is only a DT reserved node). ramlog moved there and header offsets 44/48/60 set so stock prints our ring.
- Still no mainline text in `/proc/last_kmsg` after fallback. Open: LK/preloader may clear the buffer on watchdog reset, or mainline never reaches console_initcall.

## Run 30 (eMMC raw log) - BREAKTHROUGH
- `mt8163-emmclog.c` writes printk to expdb sectors via polled MSDC0 PIO; `try-kernel` zeroes the area before, reads 256 KiB after the fallback into `local/out/last-boot.log`. 74 KB of mainline log came back.
- CPUXGPT enable fixed the counter (`cntvct 0 -> 2` at boot, USB PHY `delta=26029`, udelay fine). Mainline boots to the pmOS initramfs (stage 2).
- PHY binds; musb-hdrc fails `DMA controller not set` (-19) => MUSB_PIO_ONLY. Initramfs then fails to mount subpartitions (eMMC not driven by mainline yet).

## Runs 31-35 - MAINLINE REACHABLE OVER USB (2026-10-18)
- MUSB needed `MUSB_PIO_ONLY`, `NCM` gadget function and `NETDEVICES` (SO_BINDTODEVICE failed with ENOPROTOOPT).
- Host never saw the gadget until the PHY sequence also applied the vendor *recover* steps (clear forced-state bits 0x68/0x69/0x6a, VBUS comparator 0x1a bit4, 0x1d bit4).
- Result: `6.12.99-p80h (up after 14s)`, host gets a cdc_ncm interface, p80hd answers on 172.16.42.1:4444.
- Log channel that made this possible: `mt8163-emmclog.c` (raw eMMC sectors of expdb, polled MSDC0 PIO) read back by `try-kernel`.

## Runs 36-50 - eMMC does not enumerate on mainline (open)
- Needed so far: `vmmc` fixed 3.0 V (card OCR 0x00078000), empty pinctrl states, clocks as the vendor tree (`msdc30_0_sel` source, `infra_msdc0` hclk, `infra_msdc3` source_cg), compatible `mediatek,mt2701-mmc` (same flags as the vendor mt8163 tag: 12-bit divider, async FIFO, PAD_TUNE0).
- Trace (patch 0008 prints every command): CMD0/CMD1 fine (OCR `c0078000`), CMD2 completes with RESP = ffffffff x4 (no start bit seen), CMD3 times out. The card most likely never answers CMD2, i.e. it is not receiving our commands correctly (CMD0/CMD1 do not need a valid CRC).
- Ruled out: CMDRRDLY/CMDRDLY sweep, PATCH_BIT1/PATCH_BIT2 set to the bootloader's values, pulls from the vendor dts, source clock choice. Bootloader register state is dumped at probe (`p80h: lk ...`): SDC_CFG bus width 8, PATCH_BIT2 04809801.
- IOCON bit 1 (response sample edge) and some PAD_TUNE values hang the tablet (interrupt storm), the watchdog then falls back to stock.

## Run 69 (adopt + full command trace)
- Hypothesis: with the bootloader's card adopted (HEAD patch 0008) the core's CMD9/CMD7 work (run 67), and the later failure is a specific command; a per-command trace (`p80h: cmdN arg rsp err`) names it. Falsified if the card enumerates or the trace shows no command after CMD3.
- Result 69: cap-mmc-hw-reset (uncommitted dts change from run 68) makes the core RST_n-reset the card in mmc_init_card, undoing the adoption; CMD10 timed out. Reverted for run 70.
- Result 69: cap-mmc-hw-reset (uncommitted dts change from run 68) makes the core RST_n-reset the card in mmc_init_card, undoing the adoption; CMD10 timed out. Reverted for run 70.
- Result 70-72: eMMC WORKS on mainline. Adopting the bootloader's card (patch 0008) + the core's own enumeration reads CID/CSD/EXT_CSD, partitions appear (24 + boot0/1/rpmb), reads OK. Auto-hang in earlier runs came from starting enumeration at probe time; it is now started by the initramfs once USB is up (`echo 1 > /sys/module/mtk_sd/parameters/go`).
- Next: rootfs onto userdata (p23), boot pmOS by label.

## Run 74 (watchdog)
- Hypothesis: the tablet always resets ~26 s after boot because mtk-wdt probe fails (-22, GIC rejects EDGE_FALLING) so nothing feeds the dog LK armed; this, not the eMMC write path, killed the writes. Falsified if it still dies at ~26 s with wdt probed.


## Run 76
- Mainline switch_root'd into the userdata pmOS (ext4 mounts) then reset ~10 s later: the init's `echo > /dev/watchdog0` loop took the dog away from the kernel pinger ("watchdog did not stop") and died with the initramfs. Removed the loop; the kernel pings HW_RUNNING dogs until userspace opens one. Falsified if it still resets ~10 s after switch_root.

## Runs 76-77 - OPENBOX RUNNING (2026-10-19)
- Mainline 6.12.99 switch_roots into the pmOS rootfs on userdata (p23, ext4 label pmOS_root, flashed + sha256-verified over the daemon), OpenRC comes up, `p80h provision` pushes xorg.conf (fbdev on simplefb), `/etc/local.d/openbox.start` (Xorg on vt7 + openbox-session) and an xterm autostart. After a cold `try-kernel` Xorg+openbox+xterm start by themselves; fb0 readback shows the xterm.
- Root causes found on the way: mtk-wdt probe failed (-22, GIC rejects edge-falling) so the dog LK armed reset the tablet ~26 s after boot; the init's `echo > /dev/watchdog0` loop then stole the dog from the kernel's pinger and died at switch_root.
- Known flake: the first `try-kernel` after a mainline run sometimes lands in the stock-kernel boot partition; repeating works.

## Run 78 - touchscreen (Silead GSLx68x)
- Hypothesis: panel is GSLx68x (`touchpanel.ic` prop, `gsl_gesture` input in stock) on I2C0 addr 0x40, INT GPIO35, RST GPIO46; firmware is built into the stock kernel (13 per-panel arrays, C805G_HK = #6) in the vendor (offset,value) pair format that mainline silead.c loads. Mainline `silead` + mt65xx I2C should then give /dev/input/event0.
- Falsify: no `silead` input device in /proc/bus/input/devices after boot, or i2c timeouts (rail off / pinmux) in dmesg.
- Result run 78/79: I2C0 (mt8173-i2c, our infracfg gates) works; Silead chip ID 0x50910000 read, firmware (C805G_HK, cut from stock boot.bin by `p80h touchfw`, shipped in the initramfs because probe runs before the rootfs) loads, `silead_ts` = /dev/input/event0, Xorg takes it through libinput. Touch events themselves not yet exercised (IRQ count 0, nobody touched the glass).

## Run 80 - mainline as the default boot image
- Hypothesis: LK boots our wrapped image from the boot partition exactly as from recovery (no signature check, seen with recovery); with the stock boot image parked in recovery (power+vol-up) a bad build still has a way back, and mainline's persistent daemon lets us keep flashing from mainline itself.
- Falsify: after `p80h install local/out/recovery-79.img` + reboot, `uname -r` is not 6.12.99 or the daemon never appears.
- Result run 80: confirmed. `p80h install <wrapped image>` writes stock boot.bin to recovery and the image to boot (readback verified); a normal reboot comes up in 6.12.99 + Openbox with the daemon on 172.16.42.1. New loop: build -> `wrap` -> `install` -> `reboot` (try-kernel is only for a stock-booted tablet). Fallback to stock: power+vol-up (recovery slot), or BROM.

## Run 81 - SD card (MSDC1)
- Hypothesis: mmc1 at 0x11240000 (IRQ 80, pins 121-126, card detect GPIO49 active-low) works unmodified in mtk-sd; the adopt hack must only apply to the non-removable host (patch 0008 now gates on MMC_CAP_NONREMOVABLE). Rail VMCH assumed enabled/placeholder.
- Falsify: `/sys/class/mmc_host/mmc1` missing, or with a card inserted no mmcblk1 appears / CMD timeouts in dmesg, or eMMC (mmc0) stops enumerating.
- Result run 81/82: `mmc1` registers (needs the state_uhs pinctrl entry too), eMMC unaffected. No card in the slot, so card enumeration is unverified.

## Run 83 - PMIC consumers
- State: mt6323 regulator driver already probes (chip ID 0x2023, 30 regulators, unnamed). Hypothesis: named regulator subnodes plus keys/rtc/pwrc nodes make the power key, RTC and poweroff work, and touch/SD get real rails (vgp2, vmch/vmc).
- Falsify: no `mtk-pmic-keys` input or /dev/rtc0, touch or mmc1 probe regressions, tablet hangs at boot.
- Result run 83: confirmed. mtk-pmic-keys input, /dev/rtc0 (mt6323-rtc), named regulators; touch and SD use vgp2/vmch/vmc; touch still probes. Note: `install` readback of a 16 MiB partition can exceed the daemon timeout once; rerun.

## Run 84 - charger (bq24297 on I2C2)
- Hypothesis: charger is a TI bq2429x at 0x6b on I2C2 (SDA/SCL pins 77/78), INT GPIO23 (EINT1), CE GPIO22 low; mainline bq24190 handles it, giving a power_supply with charge status and input current.
- Falsify: no `bq24190` power_supply in /sys/class/power_supply, or i2c transfer errors at 0x6b.
- Run 84 result 1: a gpio-hog child of the mtk pinctrl node is parsed as a pin group -> probe -22 and no I2C at all. CE is now a pinmux gpio+output-low group in the charger's pinctrl state.
- Run 84 re-analysis (stock binary, no flash): the charger is **not** a bq2429x. The stock kernel only contains a `fan5405` driver; `fan5405_init` calls `i2c_register_board_info(0, {"fan5405", 0x6a})`, and the stock dmesg shows it live (`[fan5405] [0x0]=0xd0 [0x1]=0x78 [0x2]=0x8e [0x3]=0x41 ...`). The `bq24296@6b` node in the stock DT is a leftover from other boards. Run 84's I2C2/GPIO22/23 pins were guesses and are dropped.
- Run 85 hypothesis: FAN5405 on I2C0 @0x6a, driven by mainline `bq2415x` as `ti,bq24156` (PN=0 at 0x6a), limits from the stock register dump (500 mA input, 3.7 V weak, 4.2 V, 550 mA charge, 250 mA term, 68 mOhm). Falsify: no `bq24156-0` in /sys/class/power_supply, or reg 03 reads other than 0x41.
- Result run 86 (= run 85 hypothesis, kernel-86): confirmed on first boot. `bq24156-0` power_supply, `chip bq24156, revision 1.1, vender code 010`, USB online, Charging. SD card detect on GPIO28/EINT6 (`mt-eint 6 ... 11240000.mmc cd`), reads high with no card (consistent with active-low; verify with a card).

## Run 87 - I2C1/I2C2 + accelerometer (from stock ground truth)
- Hypothesis: I2C1 (pins 57/58) holds the SY8827 at 0x60 and I2C2 (77/78) the MIR3DA at 0x26 (`p80h re i2c`, ITEMS `accelerometer.i2c = 2`); MIR3DA has chip ID 0x13 like DA280, so mainline `da280` binds. Falsify: no ACK at 0x60/0x26 or no iio device.
- Result: confirmed first boot. `p80h-hw i2cscan` bus1 = 0x60, bus2 = 0x26, reg 0x01 = 0x13, `iio:device0` da280, |a| = 9.49 m/s^2. da280 ignores `mount-matrix`; orientation 7 (x=-y, y=-x, z=-z) has to be applied in userspace (hwdb).
- Tool: `/usr/local/bin/p80h-hw` (overlay) = i2c get/set/dump/scan + /dev/mem peek/poke on the tablet.
- Preloader: boot0 dumped from mainline (`/dev/mmcblk0boot0`, 154 KB used, header `EMMC_BOOT`), boot1 empty; both read twice, sha256 in SHA256SUMS.

## Runs 88-103 - DSI/DRM display (from stock RE + golden LK register state)
- Pipeline from RE: OVL0>COLOR>CCORR>AAL>GAMMA>DITHER>RDMA0>DSI0 (mt8167 routing regs, MT8163-own mutex bits and mm gates), HX8394 init + timings from the stock LCM_DRIVER (`p80h re lcm`), no IOMMU (LK: M4U off).
- Logs: `local/logs/run-N/` (build, emmclog via `src/tools/diag-kernel.sh N`). pstore at 0x43f00000 is useless: LK rewrites its ram_console header there.
- 88-91: mm gate table must be dense (clk-mtk sizes it by count) -> -ENOENT engine clock.
- 93-99: stock DT RDMA0 IRQ (0xc7) wrong; stock /proc/interrupts says SPI 186. CCORR must relay without a CTM.
- 100: oops in mtk_disp_ovl_irq_handler: LK leaves OVL INTEN 0xe -> mask at probe (0013).
- 103 (trace points): hang right after mtk_dsi_set_interrupt_enable: LK leaves DSI running (START 1, MODE sync-pulse, INTSTA 0x80000712); handler spins on DSI_BUSY in hard IRQ. Fix 0014: stop engine, cmd mode, wait idle, clear status.
- Run 104 hypothesis: with 0013+0014 the first modeset completes, DSI-1 lit by mainline, fbcon on fb1. Falsify: emmclog ends inside mtk_dsi_*/panel or oops.

## TODO - boot below the minimum battery level
- Stock LK refuses to boot Linux when VBAT <= its CLV threshold (`[BATTERY] battery voltage(%dmV) <= CLV ! Can not Boot Linux Kernel !!`, `low power, need battery charging.`). Wanted: boot anyway, with a warning on our charge screen.
- LK loads at 0x41e00000 (file offset - 0x200); string references there are not plain literal-pool words, so the check is not located yet. Needs a real ARM/Thumb disassembly of `local/dumps/emmc-partitions/lk.bin`.
- Safer way: patch a copy, check it offline (emulator or disassembly diff), keep the stock LK recoverable (BROM path tested first) before any flash; never write lk/preloader from the running system without a verified restore route. A low-battery boot also needs the eMMC writes guarded (no installs under ~3.5 V).

## Run 122+ - desktop, battery, audio (written offline, tablet unplugged; all from stock RE)
Flash order, one block per run, kernel-122 carries all three kernel parts but each has its own falsifier:
- Battery (patch 0016, `mt6323-battery.c`): stock PMIC_IMM_GetOneChannelValue(7) = VBUF_EN 0x758 bit4, toggle bit 7 of
  0x76e, poll 0x714 bit15, code * 4 * 1800 / 32768 mV. OCV profile + Q_MAX 3000 mAh from the stock fuel gauge
  (`p80h re battery`, one table for all temperatures). Falsify: `/sys/class/power_supply/battery/voltage_now` not
  within 100 mV of `p80h-hw` reading the same register by hand, or capacity stuck/unknown. Open check: stock showed
  ZCV 3861 mV as SOC 70 % where the table says 50 % (stock SOC was coulomb-counted/RTC-saved).
- Audio (patch 0017, `mt8163-afe.c` + `mt6323-codec.c`, simple-audio-card "P80h"): AFE layout = mt8173 (DL1 0x40,
  VUL 0x80, IRQ 0x3a0..0x3b0) confirmed in the stock binary; codec sequences from `p80h re audio`
  (src/re/golden/audio-{speaker,headphone,capture}.txt). Audio MTCMOS powered by the AFE driver with the stock
  spm_mtcmos_ctrl_aud sequence if LK left it off. Fixes over the old speaker-only driver: 0x4016 bit8/9 is a toggle
  handshake against bit0/1 (stock TopCtlChangeTrigger), not a plain set. 0x4028 = 0x3c2f at 48 kHz (code reading;
  the emulator logs 0x302f because the printk leaf clobbers w0). Falsify: no card in `aplay -l`, `speaker-test -c2`
  silent with IRQ count rising (codec side) or IRQ count flat (AFE side: check AUDIO_TOP_CON0, DAC_CON0, SPM bit 24).
- Desktop (`p80h provision`): packages from src/port/packages, overlay tree src/port/overlay (xfce4 portrait/touch
  config, auto-rotate via iio-sensor-proxy + udev mount matrix, NetworkManager not touching usb0, zram swap,
  pipewire). Falsify: xfce4-panel not on screen in `p80h view`, or the daemon on 172.16.42.1 lost after provision.
- Volume keys: vol-up = keypad matrix (0,0), vol-down = PMIC home key reported as KEY_VOLUMEDOWN (stock
  kpd_pmic_rstkey_hal); xfce4-pulseaudio-plugin binds XF86AudioRaise/LowerVolume.

## Not done - needs hardware-in-the-loop RE
- Wi-Fi/BT/FM/GPS: MT8163 CONSYS (MT6627-class combo inside the SoC). Stock drives it with the vendor WMT stack
  (WMT_SOC.cfg, ROMv2_lm_patch, WIFI_RAM_CODE_8163 over the CONN MTCMOS + BTIF). No mainline driver exists for this
  generation; a port is a WMT core + wlan_gen2 + BT-over-BTIF driver. Plan: trace stock wmt power-on (conn domain,
  EMI remap, patch download) with `p80h re` the same way as audio, then a minimal WMT core that boots the patch.
  Until then only USB adapters, and the MUSB port is gadget-only here (it carries the dev link).
- Camera: GC0310/GC0329/GC2035 sensors (stock), but MT8163 SENINF + ISP/CAMSV have no mainline driver.
- GPU: **not a Mali.** Stock drives only MediaTek's in-house "Sapphire Lite" core (`yolig3d`/`g3dKmd`, DT
  `mediatek,g3d_sapphire_lite` at 0x13000000, IRQ SPI 225); the kernel has no kbase at all. Live (run 134): device ID
  0x13000020 = 0x02075017 (= `g3dCheckDeviceId`), Mali GPU_ID at 0x13040000 reads 0. No open driver or Mesa backend
  exists, so panfrost is out; desktop and browser render with llvmpipe (4 cores now).

## Runs 123-126 - clock mode
- Charge screen -> pmOS debug shell (buffyboard "keyboard only"): pmOS init enters its debug shell while Volume-Down
  is held, and the charge screen continued on the Volume-Down press. Fix: wait for all keys released; auto-continue
  after 10 s without a key.
- eMMC became mmcblk1 once the SD host probed first: DT aliases mmc0/mmc1; `install` finds partitions on any mmcblk.
- PMIC interrupt storm (~27k IRQ/s on mt-eint 150, no child IRQ ever set): the stock PMIC_EINT_SETTING registers
  EINT 24 level-high; 150 was a donor-board value. With 24 the count stays 0 at idle and the keys still report.
- The USB NCM link resets under long transfers (MUSB PIO): big writes go in 8 MiB md5-checked chunks
  (`p80h flash-rootfs`); a 2.6 GB desktop rootfs is still impractical over it.
- Run 126: `p80h kernel --clock` = initramfs-only bedside clock (src/clock/clock.c): RTC time (RTC_HCTOSYS), one
  core, one redraw a minute, power key toggles the backlight. Verified by framebuffer readback; power key on the
  hardware still to be pressed.

## Run 134 - SMP (4x A53)
- RE: stock `mt_psci_cpu_boot` = PSCI 0.1 CPU_ON (IDs 0x84000003.. from the stock DT, ATF in tee1 at 0x43000000) and
  then `spm_mtcmos_ctrl_cpu(cpu, ON)`: the ATF only records the entry, the kernel switches the core's MTCMOS. SPM per
  core n: PWR_CON 0x214+4n, L1_PDN 0x25c+8n, PWR_STATUS(_2ND) bit 9+n, WFI 0x720 bit 16+n; live: CPU1-3 PWR_CON 0x32.
- Patch 0018 (weak `psci_platform_cpu_power` hook in arm64 psci.c after CPU_ON / on kill) + `mt8163-cpu-power.c`
  (stock on/off sequence), DT: psci node + cpu1-3 `enable-method = "psci"`, disabled scpsys node to find SPM.
- Result: confirmed first boot, `smp: Brought up 1 node, 4 CPUs`. Hotplug off -> PWR_CON back to 0x32, status bit
  clear; on -> 0x4d. 120 s 4-core md5sum load after hotplug cycles, 4 parallel busybox checksum loops: 0 mismatches.
- Open: one reset ~20-40 s after boot during the first (broken) hotplug test, not reproduced in 3 tries. CPU runs at
  LK's ARMPLL 1001 MHz (pcw 0x9a000) / VPROC 1.15 V, inside the stock OPP table (1.05 GHz @ 1.15 V); no cpufreq yet
  (stock OPPs: 1.3 GHz@1.30 V .. 600 MHz@1.05 V, `opp_tbls`).

## Runs 135-138 - desktop on SMP, zram, screen-off, speaker amp
- 135: userdata desktop (xfce) boots on 4 cores. Desktop did not autostart: /etc/local.d/session.start was 0644
  (daemon PUT applied the mode only on create -> `fchmod` in files.c). PipeWire hung when started twice
  (/etc/xdg/autostart/pipewire.desktop + a stray ~/.config/autostart/p80h-audio.desktop); autostarts now in the overlay.
- Audio: AFE/PCM run (hw_ptr advances via ALSA and PipeWire), but the speaker is silent. Stock DT
  `extspkamp-gpio = <0x75>` / `Ext_Speaker_Amp_Set`: external amp enable GPIO117 -> simple-audio-amplifier in DT.
  Driving it high by hand did not make sound: codec side still open (compare with src/re/golden/audio-speaker.txt).
- zram: CONFIG_ZRAM was silently dropped (needs CONFIG_BLK_DEV; 6.12 also needs a ZRAM_BACKEND_*). The build now
  lists fragment options missing from .config. pmOS zramstart then failed in `modprobe zram` (no
  /lib/modules/<rel>): the initramfs carries modules.builtin* and runs depmod in the rootfs once per kernel.
  Result 138: /dev/zram0 1.46 GB swap at boot. (zram-init dropped: postmarketos-zram-swap is a base dependency.)
- Screen off: panel node had no `backlight`, so DPMS off left the backlight on. 138: DPMS off -> bl_power 4.
  xfce4-power-manager: DPMS off after 2 min on AC, 1 min on battery (overlay xfconf, all xfce settings now in git).
- 137: thermal (patch 0019, auxadc_thermal mt8163: one bank TS1..TS3, cal from /chosen atag,devinfo words 7-9,
  channel 11, cali 165 = stock) probes, but MSR0-2 = 0x8000 (raw 0): AUXADC delivers nothing yet. Stock also sets
  bit0 of +0x120/+0x124 in another block before starting (base not identified yet).

## Runs 139-141 - audio AFE root causes (audio parked, Wi-Fi first)
- Stock ground truth: stock booted once, `/sys/kernel/debug/mtksocaudio` + `mtksocanaaudio` dumped while playing
  (src/re/golden/stock-afe-*.txt, stock-codec-playing.txt); names -> offsets from stock's dump routine.
- Fixed: AUDIO_TOP_CON0 bits 31|14 (APB access; without them AFE_CONN0-9 ignore writes, so no route ever existed),
  NEWIF_CFG0 0x4024 = 0x330 | 8 << 12 (48 kHz; the golden 0x338 was an emulator artifact), AFE_I2S_CON1 rate field,
  speaker = HP amp + external amp (GPIO117) as stock, codec output MUX -> DEMUX (only one amp powers).
- Still silent (user, 141) and the mic loopback records only ~-70 dBFS flat noise. Parked by the user for Wi-Fi.
- `p80h deploy`: LK clears the RTC recovery request (panic in the recovery kernel came back in the boot kernel), so
  candidates are test-booted one-shot from recovery, checked (cores, rootfs, panel, touch, sound, zram) and only
  then promoted; boot/recovery refuse unverified images on every path (daemon, BROM, fastboot).

## Wi-Fi (CONSYS) bring-up plan
- Base: github.com/buddyjojo/mt6572-connsys-modules (GPL, MediaTek conn_soc + wlan_gen2 on linux-next; MT6627-class
  RF like ours; Wi-Fi client/AP, BT working there). Cloned to local/trees/mt6572-connsys-modules.
- MT8163 ground truth from the stock kernel (`mtk_wcn_consys_hw_reg_ctrl`, `spm_mtcmos_ctrl_connsys`,
  `mtk_wcn_consys_hw_init`, `HifPdmaInit`), stock DT and live registers:
  - DT: BTIF 0x1100c000 SPI 112, APDMA BTIF TX 0x11000700 SPI 110 / RX 0x11000780 SPI 111, CONSYS 0x18070000 SPI 237,
    WIFI 0x180f0000 SPI 238, Wi-Fi HIF APDMA 0x11000080 (0x54), consys reserved memory 2 MB aligned 2 MB.
  - power on: VCN18 (lp mode off), VCN28 unless co-clock; TOPRGU+0x18 |= 0x88001000 (CPU reset, key 0x88);
    topckgen+0x84 = BIT(26); SPM+0 = 0x0b160001; MTCMOS: SPM 0x280 PWR_ON 2, PWR_ON_2ND 3, wait 0x60c/0x610 bit 1,
    clear ISO 4, CLK_DIS 1, set RST_B 0, clear SRAM_PDN 8, infracfg 0x220 &= ~0xe000, wait 0x228 & 0xe000 == 0;
    poll chip id 0x18070008 == 0x8163; 0x18070110 |= BIT(18); TOPRGU+0x18 = (v & 0x77ffefff) | 0x88000000; 5 ms.
  - EMI: 0x10001320 |= 0x1000 | ((phys >> 20) - 0x400); shared area = ioremap(phys + 0x80000, 0x55c00), zeroed.
  - firmware: WIFI_RAM_CODE_8163, ROMv2_lm_patch_1_{0,1}_hdr.bin, WMT_SOC.cfg (local/dumps/stock-runtime/fw);
    NVRAM WIFI (MAC, calibration) from the stock nvram partition.

## Wi-Fi bring-up runs 1-2 (kernel 142 = modules + cfg80211 + BT; stack in src/connsys, built as modules)
- src/connsys = mt6572-connsys-modules @73e9f5c + conn_soc/mt8163 (stock power/MTCMOS/EMI sequences), HIF APDMA
  0x11000080, WIFI_RAM_CODE_8163, 6.12 cfg80211 wrappers (compat_inc/linux/cfg80211_612.h), from_timer.
- Build: `make -C local/trees/linux O=local/cache/build ... M=$PWD/src/connsys modules` (after one `make modules` for
  Module.symvers). On the tablet: /root/connsys/*.ko + mtk_stp_launcher -m 3 -p /system/etc/firmware/.
- Works: BTIF probe, consys probe (EMI remap 0x10001320 = 0x13e0 for 0x7e000000), CONN MTCMOS on (PWR_CON 0xd),
  chip id 0x8163, hw/fw ver 0x8a00 (E1), WMT over BTIF, patch 1/2 (ROMv2_lm_patch_1_1, seq 1, addr 0xf00e0000 =
  EMI) downloaded: byte-identical in the reserved region at +0xe0000, so BTIF TX + EMI remap are correct.
- Blocker: patch 2/2 (ROMv2_lm_patch_1_0, seq 2, internal RAM) - every fragment ACKed except the last (frag 122,
  816 bytes): no WMT_PATCH_EVT. CONN CPUPCR afterwards sweeps 0x75000..0xfe000 linearly (CPU running off into empty
  memory). Address/P_ADDRESS templates and the 0x40/0xc8 overrides match stock byte for byte.
- Next: compare the rest of stock mtk_wcn_soc_patch_dwn/sw_init (anything between patches, frag size, delays) and
  CONN-side settings stock applies before the download (ROM_RAM_DELSEL 0x18070114, 0x10001f00 OSC/wakeup bits).

## Wi-Fi up (runs 3-6) - cable-free link
- CFG_WMT_POWER_ON_DLM=1 (stock sends the 0x80100060 DLM power-on before the patches; without it patch 2 lands in
  unpowered internal RAM and the CONN CPU runs off). Both ROM patches then download.
- HIF DMA: mt8163 is a PDMA channel (stock HifPdmaConfig: CON bit 0 dir, bit 1 fix, 17:16 burst = 3; start = INT_EN|EN
  bit 0); the mt6572 GDMA programming timed out the first read. New wlan/os/linux/hif/ahb/mt8163/ahb_pdma.c.
- Result: wlan0 at boot (/etc/local.d/wifi.start -> /root/connsys/p80h-wifi-up.sh), scans, WPA2 works. No NVRAM yet
  (MAC 00:08:22:xx random per boot). WPA3/SAE unsupported by this firmware (mkmoto hotspot is WPA3-only).
- Link: tablet AP (ap-up.sh, wpa_supplicant mode=2) SSID p80h, WPA2, 192.168.43.1, starts at boot after wlan0;
  credentials /etc/ap.conf (0600, not in git). PC USB Wi-Fi adapter joins it (static 192.168.43.2, never-default).
  Access only via SSH keys (sshd drop-in: no passwords, AllowTcpForwarding local, PermitOpen daemon + VNC):
  `ssh -N -L 14444:172.16.42.1:4444 -L 15900:127.0.0.1:5900 root@192.168.43.1`, then P80H_DAEMON=127.0.0.1:14444.
  Verified across an unattended reboot.

## Runs 143-150 - USB OTG host (keyboard verified)
- Ground truth (stock): ID = GPIO38 (KPCOL2 pad, EINT 49, pull-up; stock DT iddig_gpio), VBUS = FAN5405 boost
  (tbl_charger_otg_vbus: CONTROL 0x30, VOLTAGE 0x8e, OPA_MODE, OTG_PL, OTG_EN; drvvbus GPIO unused), host switch =
  musb_id_pin_work: session off, U2PHYDTM1 0x6c (v|0x10)&~0x2e (session end, ID low), 55 ms, session on,
  0x6c (v&~0x10)|0x2c; force byte 0x6d |= 0x3e.
- Kernel: dr_mode otg + usb-role-switch + gpio-usb-b-connector (id-gpios GPIO38, vbus-supply), patch 0020 bq2415x
  "usb-otg-vbus" regulator doing the stock sequence (the generic set_mode(BOOST) left the chip in reset defaults),
  PHY set_mode = stock session sequence (phy-mt8163-usb.c), 0021 MUSB: PHY mode before the host session, no gadget
  D+ pullup while host (restored on the switch to device), udc_start keeps a host role (re-runs the A session),
  0022 usb-conn-gpio: VBUS before the host role. USB_MUSB_DUAL_ROLE, storage/UAS, HID, USB NICs.
- Result 150: keyboard (ROCCAT Horde AIMO) enumerates at boot and on hotplug; FS device, PIO. Open: mice, the
  host -> device switch back to the PC gadget link (untested), MUSB DMA (still PIO only).

## Branding 1 - quiet animated boot (run 151)
- Hypothesis: with `quiet loglevel=0` and no cursor, nothing but p80h-splash (src/boot/splash.c: ring + comet + breathing
  core, fb0, SIGTERM = fade out) is on the panel from mdev until session.start stops it. Falsified if kernel text or a
  cursor shows, the animation stutters (>12 ms/frame), or X comes up with a black flash longer than ~0.3 s.
- Dev loop cost: `p80h look` no longer sees kernel text; use the daemon (dmesg) or drop `quiet loglevel=0` in p80h.fragment.

## Branding 2 - bottom progress bar (run 154)
Hypothesis: a full-width bottom bar (label left, "%  ETA m:ss" right, eased fill, sweep when size unknown, slide in/out)
fed by p80hd permille datagrams reads better than ring+label. Falsifier: bar not visible on the tablet during a deploy
(only drawn where p80h-splash owns fb0: initramfs/rescue; under X it needs an X-side renderer). Host render check:
glyph baseline bug in genfont.py found and fixed (tops were clipped). Next: desktop stack decision (no custom WM).

## Desktop: xfce dropped, bar under X (run 154)
Hypothesis: Openbox + tint2 + jgmenu is the small/fast stack (no custom WM); `p80h-splash --bar` (started from the Openbox
autostart) draws only the bottom bar over X's fbdev scan-out and restores the covered pixels afterwards, so deploys show
progress on the desktop too. Falsifier: the bar never appears during `p80h push` of a >= 1 MiB file, or X content under it
stays corrupted afterwards (X not on fbdev: then it needs an X-side renderer).

## Run 154 result - Openbox desktop provisioned over USB
- Kernel 154 deployed (bar, battery fix). `p80h provision` over the cable: 49 packages + all optional apps ok; xfce session
  gone, Openbox + tint2 (ePapirus icons) + p80h-splash --bar autostart. Firefox renders pages (HOME=/root needed), 423 MiB used
  with it open. tint2 logs BadDrawable from a systray client (harmless so far). Screenshot route: `scrot` on the tablet + `get`.
- Open: bar visibility under X (modesetting, not fbdev) unverified; panel only 30 px high; no cpufreq (1001 MHz fixed);
  speaker silent; no GPU; camera/suspend untested.
