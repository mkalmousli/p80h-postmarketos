"""USB-net channel: the plain telnet root shell the pmOS initramfs/rescue image opens, else ssh."""
import re
import socket
import subprocess
import time

HOST = "172.16.42.1"
PORTS = (23,)  # the stock pmOS getty; 4444 belongs to p80hd
IAC = re.compile(rb"\xff[\xfb-\xfe].")


def _open(port: int, timeout: float = 0.6) -> bool:
    try:
        socket.create_connection((HOST, port), timeout=timeout).close()
        return True
    except OSError:
        return False


class NetChannel:
    name = "net"

    def __init__(self) -> None:
        self.port = 0

    def available(self) -> bool:
        self.port = next((p for p in PORTS if _open(p)), 0)
        return bool(self.port) or _open(22)

    def sh(self, cmd: str, timeout: float = 30) -> str:
        if not self.port:
            r = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no", f"root@{HOST}", cmd],
                               capture_output=True, text=True, timeout=timeout)
            return r.stdout + r.stderr
        return self._telnet(cmd, timeout)

    def _telnet(self, cmd: str, timeout: float) -> str:
        mark = f"__END{time.monotonic_ns()}__"
        s = socket.create_connection((HOST, self.port), timeout=5)
        s.sendall(f"{cmd}\necho {mark}\n".encode())
        buf, end = b"", time.time() + timeout
        s.settimeout(1)
        while time.time() < end and buf.count(mark.encode()) < 2:
            try:
                chunk = s.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            buf += chunk
        s.close()
        text = IAC.sub(b"", buf).decode(errors="replace").replace("\r", "")
        return text.split(f"echo {mark}\n")[-1].split(mark)[0].strip()
