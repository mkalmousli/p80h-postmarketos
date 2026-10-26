/* REBOOT <mode> (reason string for LK, or "off"); INJECT <node> <size> + raw evdev bytes into an input device. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <poll.h>
#include "p80hd.h"

void cmd_reboot(int fd, const char *arg)
{
	say(fd, "OK\n");
	progress(!strcmp(arg, "off") ? "POWERING OFF" : "RESTARTING", -1);
	sync();
	if (!strcmp(arg, "off"))
		syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, 0);
	else
		syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, arg);
	say(fd, "ERR reboot\n");
}

void cmd_inject(int fd, const char *arg)
{
	char node[256], buf[4096];
	long size;
	int f;

	if (sscanf(arg, "%255s %ld", node, &size) != 2 || size > (long)sizeof(buf) ||
	    (f = open(node, O_WRONLY)) < 0 || read_full(fd, buf, size)) {
		say(fd, "ERR inject\n");
		return;
	}
	say(fd, write_full(f, buf, size) ? "ERR write\n" : "OK\n");
	close(f);
}
