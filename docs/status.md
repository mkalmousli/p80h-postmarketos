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
