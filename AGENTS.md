# Working on this repo (humans and agents)

Goal: postmarketOS on the Teclast P80h (MT8163) with a vanilla mainline kernel. See `docs/plan.md` for the roadmap and `docs/status.md` for what works.

## Rules
- Python only (uv), no bash scripts. Every file <= 100 lines of code. Nothing private or stock-vendor in git (raw dumps live in `local/dumps/`, gitignored).
- Mainline kernel is fetched into `local/trees/linux`; our additions live in `src/kernel/` (dts, drivers, patches, config) and are copied in by `p80h kernel`. Credit donors in `kernel/CREDITS.md`.
- Argue before changing: write the hypothesis and the test that would falsify it in `docs/status.md` (bring-up log), then change one thing per run.
- Commit and push after every verified step. Remove debug markers before committing a "works" state.

## Dev loop
1. `uv run p80h kernel local/out/kernel-N.bin --initramfs <pmos initramfs> --failsafe 60`
2. wrap + `uv run p80h try-kernel local/out/recovery-N.img` (flashes recovery only; the watchdog falls back to stock after ~78 s)
3. `local/out/screen.txt` = OCR of the tablet screen from the phone camera (`p80h look`), one photo per run. Last `calling X` without `returned` names the stalled initcall.
