# kernel/

Mainline Linux (pinned tag in `src/p80h/kernel/tree.py`) plus what the MT8163 needs.
Nothing here is a fork of a vendor tree: `p80h kernel` downloads vanilla Linux and overlays this directory.

| Path | What |
|---|---|
| `dts/` | device tree: `mt8163.dtsi` (SoC), `mt8163-teclast-p80h.dts` (board) |
| `config/p80h.fragment` | options merged over arm64 `defconfig` |
| `patches/` | `git apply` patches against the pinned tag |
| `drivers/` | new files copied into the tree at the same relative path (ported drivers) |
| `CREDITS.md` | where every ported piece comes from |

Build: `uv run p80h kernel out/kernel.bin [--initramfs rootfs.cpio.gz]`
