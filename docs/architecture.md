# Architecture — how the parts fit together

## 1. The big picture
```
 HOST PC (Linux)                                        TABLET (MT8163)
┌──────────────────────────────────┐   USB cable   ┌─────────────────────────────────────┐
│ src/p80h  (uv run p80h ...)      │               │ BootROM ─▶ Preloader ─▶ LK ─▶ kernel │
│  ├ brom.py / fastboot.py ────────┼──BROM/preldr──┤        (mask ROM, unlockable)        │
│  ├ channels/ daemon · adb · net ─┼──USB-NCM─────▶│ initramfs ─▶ p80hd (port 4444)       │
│  ├ kernel/ + image/ (build)      │  172.16.42.1  │ eMMC: boot, recovery, userdata ...   │
│  └ trykernel.py (flash+observe)  │               └─────────────────────────────────────┘
└─────────────▲────────────────────┘
              │ reads / writes
 ┌────────────┴───────────────────────────────────────────────┐
 │ src/kernel (dts, patches, config)  ──copied into──▶ local/trees/linux (mainline 6.12.99)
 │ src/daemon (C)  ──cross-compiled──▶ p80hd ──packed with pmOS initramfs──▶ boot.img
 │ local/dumps/  (stock backup: restore path)    local/out/  (built images, logs)
 └──────────────────────────────────────────────────────────────
```

## 2. Boot chain on the tablet
```
 power ─▶ BootROM ─▶ Preloader ─▶ LK (little kernel) ─▶ boot.img / recovery.img
          (BROM mode lets the host write eMMC raw: mtkclient — the unbrick path)
                                                     │
                       kernel (stock 3.10 or our mainline 6.12) ─▶ initramfs ─▶ p80hd starts
                                                     │                         (before rootfs)
                                                     └─▶ switch_root ─▶ Alpine/postmarketOS on userdata
```
Partitions that matter: `boot`, `recovery` (test kernels go here), `userdata` (rootfs), `expdb` (raw log channel), `lk`, `seccfg`, `nvram`/`proinfo`/`persist` (device identity — backed up in `local/dumps/emmc-partitions/`).

## 3. The dev loop (one run)
```
 edit src/kernel/{dts,patches,config}
        │  p80h kernel local/out/kernel-N.bin
        ▼
 fetch/patch/build mainline  ─▶  wrap for LK (+ pmOS initramfs + p80hd)  ─▶ recovery-N.img
        │  p80h try-kernel
        ▼
 flash recovery ONLY ─▶ tablet boots it ─┬─ OK:   p80hd answers over USB ─▶ inspect live
                                         └─ hang: watchdog ~78 s ─▶ falls back to stock boot
        ▼
 read back logs: expdb raw eMMC log ─▶ local/out/last-boot.log
        ▼
 record hypothesis → result in docs/status.md, commit
```

## 4. Control channels (best first, `src/p80h/channels/`)
| Channel | Transport | Needs | Used for |
|---|---|---|---|
| daemon | TCP 172.16.42.1:4444 over USB-NCM | boot.img with `p80hd` | exec, shell, put/get, partition write, input inject, reboot |
| adb | USB | stock Android | surveys of the stock system |
| net | USB-net shells | any initramfs with a shell | fallback |
| brom | USB, preloader window | power-cycle w/ cable | raw flash/restore (mtkclient), always works |

## 5. p80hd daemon (`src/daemon/`, see `daemon.md`)
```
 supervisor ──owns──▶ listening socket ──▶ worker ──fork per connection (≤16)──▶ EXEC | SHELL | PUT | GET | STATUS | REBOOT | INJECT
     └─ restarts worker on crash                         runs in /proc/1/root → survives switch_root
```

## 6. Status map (what blocks what)
```
 BROM access ✅ → stock backup ✅ → mainline boots ✅ → USB-NCM + p80hd ✅ → eMMC ❌ (CMD2 no reply)
                                                                               │ blocks ▼
                          rootfs on userdata → PMIC/regulators → touch/keys/backlight/battery → display → Wi-Fi/BT → GPU
```
Live details: `status.md` (log) · `plan.md` (roadmap) · `hardware.md` (chips).
