"""Channel to p80hd, the daemon on the tablet (started from the boot image, so it is up before any system)."""
import json
import os
import re
import socket

TRAILER = re.compile(r"\n@@(EXIT (\d+)|TIMEOUT)\n$")


def endpoint() -> tuple[str, int]:
    host, _, port = os.environ.get("P80H_AGENT", "172.16.42.1:4444").partition(":")
    return host, int(port)


class AgentChannel:
    name = "agent"

    def conn(self, header: str, timeout: float = 30) -> socket.socket:
        s = socket.create_connection(endpoint(), timeout=timeout)
        s.sendall(header.encode() + b"\n")
        return s

    def _all(self, s: socket.socket) -> bytes:
        buf = b""
        while chunk := s.recv(65536):
            buf += chunk
        s.close()
        return buf

    def available(self) -> bool:
        try:
            return self._all(self.conn("PING", 1.5)).startswith(b"p80hd")
        except OSError:
            return False

    def run(self, cmd: str, timeout: float = 30) -> tuple[str, int]:
        """Run a shell command; returns (output, exit code). Exit 124 means the device-side timeout fired."""
        out = self._all(self.conn(f"EXEC {int(timeout)} " + cmd.replace("\n", "; "), timeout + 5)).decode(errors="replace")
        m = TRAILER.search(out)
        return TRAILER.sub("", out).strip(), (124 if m and m.group(2) is None else int(m.group(2))) if m else -1

    def sh(self, cmd: str, timeout: float = 30) -> str:
        return self.run(cmd, timeout)[0]

    def status(self) -> dict:
        return json.loads(self._all(self.conn("STATUS", 5)))

    def put(self, path: str, data: bytes, mode: int = 0o644) -> None:
        s = self.conn(f"PUT {path} {len(data)} {mode:o}", 600)
        s.sendall(data)
        if (reply := self._all(s)) != b"OK\n":
            raise OSError(f"put {path}: {reply.decode().strip()}")

    def get(self, path: str, offset: int = 0, length: int = -1) -> bytes:
        reply = self._all(self.conn(f"GET {path} {offset}" + (f" {length}" if length >= 0 else ""), 600))
        head, _, body = reply.partition(b"\n")
        if not head.isdigit():
            raise OSError(f"get {path}: {head.decode()}")
        return body

    def inject(self, node: str, data: bytes) -> None:
        s = self.conn(f"INJECT {node} {len(data)}")
        s.sendall(data)
        if (reply := self._all(s)) != b"OK\n":
            raise OSError(f"inject {node}: {reply.decode().strip()}")

    def reboot(self, mode: str) -> None:
        try:
            self._all(self.conn(f"REBOOT {mode}", 5))
        except OSError:
            pass
