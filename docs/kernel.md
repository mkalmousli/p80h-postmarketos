# Kernel plan

The stock 3.10.72 vendor kernel has no devtmpfs, VT/fbcon, uinput or namespaces, and we have no source for it.
The MediaTek reference tablet tree used for the Fire HD 8 2017 (MT8163, 3.18, arm64) is the closest buildable base:
the P80h's chips (Goodix touch, bq24296 charger, MC3XXX/BMA222 accelerometers, CONSYS Wi-Fi/BT) match its `tb8163p3_64_basic` board.

Stages:
1. **First light**: build the tree with our config (devtmpfs, uinput, VT, namespaces), keep the panel the way LK left it
   (simplefb on the LK framebuffer, no LCM driver needed), boot the existing initramfs, reach the daemon.
2. **Input and power**: touch, keys, backlight, battery/charger, sensors.
3. **Wireless**: CONSYS Wi-Fi/Bluetooth with the firmware from the stock system.
4. **Own display driver**: port the `c805_zs_hx8394a01_inx_wxga_ips_8` panel init (read from the stock kernel) if power management needs it.

Every stage is flashed to `boot` through the daemon; the BROM path always restores the previous image.

## Log channels (bring-up)
- RAM log: `mt8163-ramlog` mirrors the console into the third page of the stock framebuffer (phys 0x7f3d0000); after the watchdog reset `try-kernel` scans `/dev/fb0` for `P80HLOG` and writes `local/out/last-boot.log`. Fast and complete.
