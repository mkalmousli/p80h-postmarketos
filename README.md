# Teclast P80h (p80h_d8c1_g) — open-source bring-up: Alpine/postmarketOS on mainline Linux

Self-contained: everything lives in 3 folders. **Agents: read this, then `docs/architecture.md` and `docs/status.md`. Don't scan the tree.**

Device: Teclast P80h, MediaTek **MT8163** (4x Cortex-A53), 8" 800x1280, eMMC 7.75 GB, stock Android 5.1 / kernel 3.10.72 / LK (unlocked via BROM + mtkclient).
Target: mainline 6.12.99 + postmarketOS (Alpine), full hardware access.
State: mainline boots to initramfs, reachable over USB-NCM (`p80hd` at 172.16.42.1:4444). **Blocker: eMMC doesn't enumerate on mainline** (CMD2 gets no response).

## `src/` — everything we write (git)
| Path | What |
|---|---|
| `p80h/` | Python CLI (`uv run p80h ...`): brom, kernel build, image build, try-kernel |
| `kernel/` | our dts (`dts/`), patch series (`patches/`), `config/p80h.fragment`, drivers, CREDITS.md |
| `daemon/` | C source of `p80hd`, the on-tablet daemon (root service over USB-net) |
| `port/` | `pmaports/` pmOS device+kernel pkgs · `overlay/` rootfs tree mirrored onto / and `packages` (apk list), both applied by `p80h provision` · `stock/` stock boot inputs |
| `tools/` | `devmem.c`, `pmbootstrap.cfg`, `retry-scripts/` (OLD bash, hardcode dead paths) |
| `tests/` | pytest |

## `docs/` — knowledge (git)
`architecture.md` diagrams: boot chain, dev loop, channels · `status.md` bring-up log (newest last) · `todo.md` full driver checklist · `plan.md` short roadmap · `hardware.md` subsystem table · `kernel.md` · `daemon.md` · `guide.md` how-to · `journal/` dated RE notes (`YYYY-MM-DD-topic.md`). Rules: `AGENTS.md` (Python/uv only, files ≤100 LOC).

## `local/` — big/private, gitignored (MAC/serial/calibration inside)
| Path | What |
|---|---|
| `dumps/emmc-partitions/` | **stock backup** (raw BROM dumps + `SHA256SUMS`) — never edit; this is the recovery path |
| `dumps/stock-runtime/` | stock Android survey: cpuinfo, iomem, dmesg, getprop, config.gz, `fw/` blobs |
| `dumps/stock-extracted/` | unpacked stock ramdisk, crash/expdb snapshot |
| `dumps/logs/` | old session logs (w*.log, *.status) |
| `re/` | decompiled stock DTB (`dt/`), early probe boot image |
| `trees/` | `linux` (mainline v6.12.99 work tree) |
| `vendor/` | `mtkclient` (set `P80H_MTK`), `pmbootstrap`, `pmaports` (use `src/tools/pmbootstrap.cfg`) |
| `out/` | built kernels/images/logs (`last-boot.log`), `legacy-images/` |
| `build/pmos-work/` | pmbootstrap chroots · `cache/` kernel build cache |

Safety: a hung test kernel falls back to stock after ~78 s (`try-kernel` flashes recovery only).
