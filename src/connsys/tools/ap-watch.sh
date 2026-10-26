#!/bin/sh
# ap-watch.sh: keep the tablet's AP (the PC link) usable. After some boots ap0 reports COMPLETED but is not on the
# air; nothing then ever associates. Restart the AP whenever no station has been associated for 3 minutes.
idle=0
while sleep 30; do
	if [ -d /sys/class/net/ap0 ] && [ -n "$(iw dev ap0 station dump 2>/dev/null)" ]; then
		idle=0
		continue
	fi
	idle=$((idle + 30))
	[ $idle -lt 180 ] && continue
	echo "ap-watch: no client for ${idle}s, restarting the AP" > /dev/kmsg
	/root/connsys/ap-up.sh stop
	sleep 2
	/root/connsys/ap-up.sh start >> /var/log/wifi-up.log 2>&1
	idle=0
done
