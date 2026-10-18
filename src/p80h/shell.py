"""Interactive terminal on the tablet: raw local tty <-> the daemon's SHELL pty."""
import os
import select
import shutil
import sys
import termios
import tty

from .channels.daemon import DaemonChannel


def interactive() -> None:
    size = shutil.get_terminal_size()
    s = DaemonChannel().conn(f"SHELL {size.lines} {size.columns}", 10)
    if s.recv(3) != b"OK\n":
        sys.exit("daemon refused the shell")
    fd, old = sys.stdin.fileno(), termios.tcgetattr(sys.stdin)
    tty.setraw(fd)
    try:
        while True:
            ready, _, _ = select.select([s, fd], [], [])
            if s in ready:
                data = s.recv(4096)
                if not data:
                    break
                os.write(sys.stdout.fileno(), data)
            if fd in ready:
                s.sendall(os.read(fd, 4096))
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
