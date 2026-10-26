#!/bin/sh
# p80h-wifi-up.sh: bring up the MT8163 CONSYS stack and wlan0 (modules + launcher in /root/connsys, firmware in
# /system/etc/firmware and /lib/firmware). Idempotent; prints WLAN0_UP <mac> or the WMT errors.
cd /root/connsys || exit 1
[ "$1" = -v ] && dmesg -n 8
for m in mtk_btif_drv mtk_stp_wmt_soc mtk_stp_bt_soc mtk_wmt_wifi_soc wlan_gen2; do
	lsmod | grep -q "^$m " || insmod $m.ko || { echo "insmod $m failed"; exit 1; }
done
[ -c /dev/stpwmt ] || mknod /dev/stpwmt c 190 0
[ -c /dev/stpbt ] || mknod /dev/stpbt c 192 0
[ -c /dev/wmtWifi ] || mknod /dev/wmtWifi c 155 0
pgrep -f mtk_stp_launcher >/dev/null || { ./mtk_stp_launcher -m 3 -p /system/etc/firmware/ >/tmp/launcher.log 2>&1 & sleep 2; }
[ "$1" = -v ] && echo "9 3" > /proc/driver/wmt_dbg
echo 1 > /dev/wmtWifi || echo "wmtWifi on failed"
for i in $(seq 1 20); do [ -d /sys/class/net/wlan0 ] && break; sleep 1; done
if [ -d /sys/class/net/wlan0 ]; then
	echo "WLAN0_UP $(cat /sys/class/net/wlan0/address)"
else
	echo "WLAN0_MISSING"; dmesg | grep -E "\]\[E\]|\]\[W\]" | grep -v "^\[.*\] 0x" | tail -15
	exit 1
fi
