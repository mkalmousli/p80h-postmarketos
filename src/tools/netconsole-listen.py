#!/usr/bin/env python3
"""netconsole-listen OUT: append every kernel netconsole datagram (UDP 6666) to OUT, line-buffered."""
import socket
import sys

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 6666))
with open(sys.argv[1], "ab", buffering=0) as out:
    while True:
        out.write(s.recv(65535))
