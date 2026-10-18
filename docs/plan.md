# Plan

1. USB gadget + RNDIS up on mainline, so `p80hd` answers (in progress; clocks fixed, PHY sequence being bisected).
2. eMMC (mmc0) -> rootfs on userdata.
3. PMIC (MT6323 via pwrap), regulators, power domains.
4. Touch, keys, backlight, battery/charger, sensors.
5. Display (mmsys/DRM), then Wi-Fi/BT.
6. `p80h install` one command (backup -> flash -> rootfs -> verify), Xfce.
7. Clean: drop debug markers/initcall_debug, files <= 100 lines, CREDITS, docs.
