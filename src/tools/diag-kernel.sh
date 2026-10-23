#!/bin/sh
# diag-kernel.sh N [CMDLINE_EXTRA]: diagnostic build N = normal config + raw eMMC log (mt8163-emmclog owns MSDC0,
# so mmc0 is disabled and the initramfs parks), flashed to recovery and booted once. After the fallback boot,
# `p80h emmclog N` saves the log to local/logs/run-N/emmclog.txt. The working tree is restored afterwards.
set -e
N=$1; EXTRA=$2
FRAG=src/kernel/config/p80h.fragment; DTS=src/kernel/dts/mt8163-teclast-p80h.dts
T=$(mktemp -d); cp $FRAG $DTS $T/; trap 'cp $T/p80h.fragment $FRAG; cp $T/mt8163-teclast-p80h.dts $DTS; rm -rf $T' EXIT
sed -i 's/^# CONFIG_MT8163_EMMCLOG is not set.*/CONFIG_MT8163_EMMCLOG=y/' $FRAG
[ -n "$EXTRA" ] && sed -i "s/^CONFIG_CMDLINE=\"\(.*\)\"/CONFIG_CMDLINE=\"\1 $EXTRA\"/" $FRAG
python3 -I -c "
p='$DTS'; s=open(p).read(); i=s.index('&mmc0 {'); j=s.index('\n};', i)
open(p, 'w').write(s[:j] + '\n\tstatus = \"disabled\";' + s[j:])"
mkdir -p local/logs/run-$N
uv run p80h kernel local/out/kernel-$N.bin --rescue --initramfs local/build/pmos-work/chroot_rootfs_teclast-p80h/boot/initramfs > local/logs/run-$N/build.log 2>&1
grep CONFIG_CMDLINE= local/cache/build/.config > local/logs/run-$N/cmdline.txt
uv run p80h wrap local/out/kernel-$N.bin
uv run p80h emmclog clear
uv run p80h flash --via daemon recovery=local/out/recovery-$N.img
uv run p80h reboot recovery
