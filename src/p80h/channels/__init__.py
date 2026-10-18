"""Shell channels, best first: the p80hd daemon, adb, then plain USB-net shells."""
from .adb import AdbChannel
from .daemon import DaemonChannel
from .net import NetChannel

ALL = (DaemonChannel, AdbChannel, NetChannel)
