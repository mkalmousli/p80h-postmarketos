"""Abuse, concurrency, crash recovery, pty."""
import os
import signal
import socket
import subprocess
import time
import unittest
from pathlib import Path

from agentbase import PORT, AgentBase



class Robust(AgentBase):
    def test_put_short_upload_leaves_no_file(self):
        path = str(Path(self.tmp.name, "short"))
        s = self.ch.conn(f"PUT {path} 1000 644")
        s.sendall(b"abc")
        s.shutdown(socket.SHUT_WR)
        self.assertEqual(s.recv(100), b"ERR short\n")
        self.assertFalse(os.path.exists(path) or os.path.exists(path + ".p80h-tmp"))

    def test_put_never_creates_under_dev(self):
        with self.assertRaises(OSError):
            self.ch.put("/dev/p80h-no-such-node", b"x")
        self.assertFalse(os.path.exists("/dev/p80h-no-such-node"))

    def test_garbage_and_overlong_lines(self):
        for junk in (os.urandom(5000), b"X" * 10000, b"\x00\xff\n", b"PUT\n", b"GET\n", b"INJECT\n"):
            s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
            s.sendall(junk)
            s.close()
        self.assertTrue(self.ch.available())

    def test_many_parallel_connections(self):
        socks = [self.ch.conn("EXEC 5 sleep 1") for _ in range(8)]
        self.assertTrue(self.ch.available())
        for s in socks:
            s.close()

    def test_survives_worker_crash(self):
        pids = subprocess.run(["pgrep", "-P", str(self.proc.pid)], capture_output=True, text=True).stdout.split()
        os.kill(int(pids[0]), signal.SIGKILL)
        for _ in range(40):
            time.sleep(0.25)
            if self.ch.available():
                return
        self.fail("agent did not come back after its worker was killed")

    def test_pty_shell(self):
        s = self.ch.conn("SHELL 24 80")
        if s.recv(3) != b"OK\n":
            self.skipTest("no devpts here")
        s.sendall(b"echo marker$((6*7))\n")
        buf, end = b"", time.time() + 5
        while b"marker42" not in buf.replace(b"echo marker$((6*7))", b"") and time.time() < end:
            buf += s.recv(4096)
        s.close()
        self.assertIn(b"marker42", buf.replace(b"echo marker$((6*7))", b""))


if __name__ == "__main__":
    unittest.main()
