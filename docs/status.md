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
