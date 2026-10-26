#!/bin/sh
# chunked, verified, resumable write of root-desktop.img onto userdata (run from the rescue initramfs)
B=busybox; U=http://172.16.42.2:8180/chunks; D=/dev/mmcblk0p23; N=311
$B wget -q -O /tmp/index $U/index || exit 1
for n in $($B seq 0 $((N - 1))); do
	sum=$($B awk -v n=$n '$1 == n {print $2}' /tmp/index)
	if [ -z "$sum" ]; then
		$B dd if=/dev/zero of=$D bs=1M count=8 seek=$((n * 8)) conv=notrunc 2>/dev/null
	else
		f=$(printf %04d $n)
		until $B wget -q -T 20 -O /tmp/c.gz $U/$f.gz && $B gunzip -c /tmp/c.gz > /tmp/c && [ "$($B md5sum /tmp/c | cut -d' ' -f1)" = "$sum" ]; do sleep 1; done
		$B dd if=/tmp/c of=$D bs=1M seek=$((n * 8)) conv=notrunc 2>/dev/null
	fi
	echo "$n/$N" > /tmp/flash.progress
done
sync; echo done > /tmp/flash.progress
