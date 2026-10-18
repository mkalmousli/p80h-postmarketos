"""Black-box tests: build p80hd natively, run it on loopback, drive it through the real client classes."""
import json
import os
import signal
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from p80h.channels.daemon import DaemonChannel

DAEMON = Path(__file__).resolve().parents[1] / "daemon"
PORT = 14000 + os.getpid() % 1000


class DaemonBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        exe = Path(cls.tmp.name, "p80hd")
        subprocess.run(["gcc", "-Wall", "-O1", "-o", str(exe), *map(str, DAEMON.glob("*.c"))], check=True)
        os.environ["P80H_DAEMON"] = f"127.0.0.1:{PORT}"
        cls.proc = subprocess.Popen([str(exe), "-b", "127.0.0.1", "-p", str(PORT)], start_new_session=True)
        cls.ch = DaemonChannel()
        for _ in range(50):
            if cls.ch.available():
                break
            time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        os.killpg(cls.proc.pid, signal.SIGKILL)
        cls.tmp.cleanup()
