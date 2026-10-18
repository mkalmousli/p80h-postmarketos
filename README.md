# postmarketOS on the Teclast P80h (MT8163)

Experimental port of postmarketOS to the Teclast P80h tablet (MediaTek MT8163, 8", 800x1280, Android 5.1,
stock 3.10.72 kernel). Everything here was done from Linux with adb, fastboot and
[mtkclient](https://github.com/bkerler/mtkclient). **Status: see [docs/status.md](docs/status.md).**

* [docs/guide.md](docs/guide.md) – step-by-step, with the dead ends we hit
* `scripts/` – numbered scripts for each step
* `port/` – pmaports device + kernel packages (kernel is extracted from *your own* boot dump, no blobs in this repo)

**Warning:** unlocking the bootloader wipes the tablet. Back up first and keep the backup private
(it contains MAC/serial/calibration data). You can brick your device; no warranty.
