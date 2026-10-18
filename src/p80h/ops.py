"""Operations that need the p80hd agent: files, partitions, screenshots, input. Mixed into Tablet."""
from pathlib import Path

from . import inputs


class AgentOps:
    def agent(self):
        ch = self.channel()
        if not ch or ch.name != "agent":
            raise RuntimeError("p80hd agent not reachable (needs a boot image that contains it)")
        return ch

    def put(self, path: str, data: bytes) -> None:
        self.agent().put(path, data)

    def get(self, path: str) -> bytes:
        return self.agent().get(path)

    def partition(self, name: str) -> str:
        """Device node of a GPT partition by name (needs the nodes mdev created)."""
        cmd = f"grep -l PARTNAME={name}$ /sys/class/block/mmcblk0p*/uevent | head -1"
        return "/dev/" + self.sh(cmd).split("/")[4]

    def flash(self, name: str, data: bytes) -> None:
        """Write a partition through the agent and verify by reading back its size."""
        node = self.partition(name)
        self.put(node, data)
        if len(self.get(node)[: len(data)]) != len(data):
            raise OSError(f"flash {name}: readback short")

    def screenshot(self) -> bytes:
        """PNG of the current framebuffer page (yoffset read from the driver)."""
        from . import screen
        y = int(self.sh("cat /sys/class/graphics/fb0/pan 2>/dev/null || echo 0,0").split(",")[-1] or 0)
        return screen.from_fb(self.get("/dev/fb0"), y)

    def push(self, src: Path, dest: str = "/") -> int:
        """Copy a directory tree onto the tablet (keeps the executable bit); returns the file count."""
        files = [f for f in sorted(src.rglob("*")) if f.is_file()]
        for f in files:
            target = dest.rstrip("/") + "/" + f.relative_to(src).as_posix()
            self.sh(f"mkdir -p $(dirname {target})")
            self.put(target, f.read_bytes())
            if f.stat().st_mode & 0o111:
                self.sh(f"chmod +x {target}")
        return len(files)

    def key(self, name: str) -> None:
        code = inputs.KEYS.get(name) or int(name)
        self._inject("gpio|kpd|keypad|mtk-kpd", inputs.key_events(code))

    def tap(self, x: int, y: int) -> None:
        self._inject("gsl|tpd|touch", inputs.touch_events([(x, y)]))

    def _inject(self, name_re: str, data: bytes) -> None:
        node = inputs.find_node(self.sh, name_re)
        ch = self.channel()
        if ch and ch.name == "agent":
            ch.inject(node, data)
        else:
            inputs.send(self.sh, node, data)
