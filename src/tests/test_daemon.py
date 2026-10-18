"""Basic daemon commands."""
import os
import signal
import socket
import subprocess
import time
import unittest
from pathlib import Path

from daemonbase import PORT, DaemonBase



class Basic(DaemonBase):
    def test_ping_status(self):
        self.assertTrue(self.ch.available())
        st = self.ch.status()
        self.assertIn(st["stage"], ("rootfs", "initramfs"))
        self.assertGreater(st["uptime"], 0)

    def test_exec_output_and_exit_code(self):
        self.assertEqual(self.ch.run("echo hi; exit 3"), ("hi", 3))

    def test_exec_timeout_kills_process_group(self):
        t0 = time.time()
        out, code = self.ch.run("sleep 30 & sleep 30", timeout=1)
        self.assertEqual(code, 124)
        self.assertLess(time.time() - t0, 6)

    def test_background_child_does_not_hold_connection(self):
        t0 = time.time()
        self.ch.run("sleep 5 >/dev/null 2>&1 &")
        self.assertLess(time.time() - t0, 3)

    def test_put_get_roundtrip_atomic(self):
        path = str(Path(self.tmp.name, "blob"))
        data = os.urandom(300_000)
        self.ch.put(path, data, 0o755)
        self.assertEqual(self.ch.get(path), data)
        self.assertEqual(self.ch.get(path, 100, 50), data[100:150])
        self.assertEqual(oct(os.stat(path).st_mode)[-3:], "755")
        self.assertFalse(os.path.exists(path + ".p80h-tmp"))


if __name__ == "__main__":
    unittest.main()
