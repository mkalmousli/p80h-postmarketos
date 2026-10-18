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
- `mt8163-emmclog.c` writes printk to expdb sectors via polled MSDC0 PIO; `try-kernel` zeroes the area before, reads 256 KiB after the fallback into `out/last-boot.log`. 74 KB of mainline log came back.
- CPUXGPT enable fixed the counter (`cntvct 0 -> 2` at boot, USB PHY `delta=26029`, udelay fine). Mainline boots to the pmOS initramfs (stage 2).
- PHY binds; musb-hdrc fails `DMA controller not set` (-19) => MUSB_PIO_ONLY. Initramfs then fails to mount subpartitions (eMMC not driven by mainline yet).

## Runs 31-35 - MAINLINE REACHABLE OVER USB (2026-10-18)
- MUSB needed `MUSB_PIO_ONLY`, `NCM` gadget function and `NETDEVICES` (SO_BINDTODEVICE failed with ENOPROTOOPT).
- Host never saw the gadget until the PHY sequence also applied the vendor *recover* steps (clear forced-state bits 0x68/0x69/0x6a, VBUS comparator 0x1a bit4, 0x1d bit4).
- Result: `6.12.99-p80h (up after 14s)`, host gets a cdc_ncm interface, p80hd answers on 172.16.42.1:4444.
- Log channel that made this possible: `mt8163-emmclog.c` (raw eMMC sectors of expdb, polled MSDC0 PIO) read back by `try-kernel`.
