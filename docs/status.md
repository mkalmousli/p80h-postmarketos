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
- Hypothesis: panel is GSLx68x (`touchpanel.ic` prop, `gsl_gesture` input in stock) on I2C0 addr 0x40, INT GPIO35, RST GPIO46 (vendor douglas.dts CTP pins); firmware is built into the stock kernel (13 per-panel arrays, C805G_HK = #6) in the vendor (offset,value) pair format that mainline silead.c loads. Mainline `silead` + mt65xx I2C should then give /dev/input/event0.
- Falsify: no `silead` input device in /proc/bus/input/devices after boot, or i2c timeouts (rail off / pinmux) in dmesg.
- Result run 78/79: I2C0 (mt8173-i2c, our infracfg gates) works; Silead chip ID 0x50910000 read, firmware (C805G_HK, cut from stock boot.bin by `p80h touchfw`, shipped in the initramfs because probe runs before the rootfs) loads, `silead_ts` = /dev/input/event0, Xorg takes it through libinput. Touch events themselves not yet exercised (IRQ count 0, nobody touched the glass).

## Run 80 - mainline as the default boot image
- Hypothesis: LK boots our wrapped image from the boot partition exactly as from recovery (no signature check, seen with recovery); with the stock boot image parked in recovery (power+vol-up) a bad build still has a way back, and mainline's persistent daemon lets us keep flashing from mainline itself.
- Falsify: after `p80h install local/out/recovery-79.img` + reboot, `uname -r` is not 6.12.99 or the daemon never appears.
- Result run 80: confirmed. `p80h install <wrapped image>` writes stock boot.bin to recovery and the image to boot (readback verified); a normal reboot comes up in 6.12.99 + Openbox with the daemon on 172.16.42.1. New loop: build -> `wrap` -> `install` -> `reboot` (try-kernel is only for a stock-booted tablet). Fallback to stock: power+vol-up (recovery slot), or BROM.

## Run 81 - SD card (MSDC1)
- Hypothesis: mmc1 at 0x11240000 (IRQ 80, pins 121-126, card detect GPIO49 active-low, from the vendor douglas.dts) works unmodified in mtk-sd; the adopt hack must only apply to the non-removable host (patch 0008 now gates on MMC_CAP_NONREMOVABLE). Rail VMCH assumed enabled/placeholder.
- Falsify: `/sys/class/mmc_host/mmc1` missing, or with a card inserted no mmcblk1 appears / CMD timeouts in dmesg, or eMMC (mmc0) stops enumerating.
- Result run 81/82: `mmc1` registers (needs the state_uhs pinctrl entry too), eMMC unaffected. No card in the slot, so card enumeration is unverified.

## Run 83 - PMIC consumers
- State: mt6323 regulator driver already probes (chip ID 0x2023, 30 regulators, unnamed). Hypothesis: named regulator subnodes (vendor names from douglas.dts) plus keys/rtc/pwrc nodes make the power key, RTC and poweroff work, and touch/SD get real rails (vgp2, vmch/vmc).
- Falsify: no `mtk-pmic-keys` input or /dev/rtc0, touch or mmc1 probe regressions, tablet hangs at boot.
- Result run 83: confirmed. mtk-pmic-keys input, /dev/rtc0 (mt6323-rtc), named regulators; touch and SD use vgp2/vmch/vmc; touch still probes. Note: `install` readback of a 16 MiB partition can exceed the daemon timeout once; rerun.

## Run 84 - charger (bq24297 on I2C2)
- Hypothesis: charger is a TI bq2429x at 0x6b on I2C2 (SDA/SCL pins 77/78), INT GPIO23 (EINT1), CE GPIO22 low (vendor douglas.dts); mainline bq24190 handles it, giving a power_supply with charge status and input current.
- Falsify: no `bq24190` power_supply in /sys/class/power_supply, or i2c transfer errors at 0x6b.
- Run 84 result 1: a gpio-hog child of the mtk pinctrl node is parsed as a pin group -> probe -22 and no I2C at all. CE is now a pinmux gpio+output-low group in the charger's pinctrl state.
- Run 84 re-analysis (stock binary, no flash): the charger is **not** a bq2429x. The stock kernel only contains a `fan5405` driver; `fan5405_init` calls `i2c_register_board_info(0, {"fan5405", 0x6a})`, and the stock dmesg shows it live (`[fan5405] [0x0]=0xd0 [0x1]=0x78 [0x2]=0x8e [0x3]=0x41 ...`). The `bq24296@6b` node in the stock DT and douglas' bq24297 are leftovers from other boards. Run 84's I2C2/GPIO22/23 pins were douglas guesses and are dropped.
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
