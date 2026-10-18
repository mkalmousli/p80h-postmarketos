"""Shell channels, best first: the p80hd agent, adb, then plain USB-net shells."""
from .adb import AdbChannel
from .agent import AgentChannel
from .net import NetChannel

ALL = (AgentChannel, AdbChannel, NetChannel)
