#!/bin/sh
# share-net.sh [off]: give the tablet this PC's internet via NAT (USB-NCM 172.16.42.0/24, or P80H_NET=192.168.43.0/24 for
# the tablet's Wi-Fi AP link). Idempotent; "off" removes it.
NET=${P80H_NET:-172.16.42.0/24}
rule() {	# rule TABLE CHAIN SPEC...
	t=$1; c=$2; shift 2
	if [ "$MODE" = off ]; then sudo iptables -t $t -D $c "$@" 2>/dev/null
	else sudo iptables -t $t -C $c "$@" 2>/dev/null || sudo iptables -t $t -A $c "$@"; fi
}
MODE=$1
rule nat POSTROUTING -s $NET ! -d $NET -j MASQUERADE
rule filter FORWARD -s $NET -j ACCEPT
rule filter FORWARD -d $NET -j ACCEPT
[ "$MODE" = off ] || sudo sysctl -qw net.ipv4.ip_forward=1
