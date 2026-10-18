/* STATUS: one JSON line describing the running system, for the host to decide what it is talking to. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "p80hd.h"

static void field(char *out, long max, const char *path, int word)
{
	char buf[256];
	char *p = buf;

	slurp(path, buf, sizeof(buf));
	while (word--)
		p = strchr(p, ' ') ? strchr(p, ' ') + 1 : p;
	snprintf(out, max, "%.*s", (int)strcspn(p, " \n"), p);
}

void cmd_status(int fd, const char *arg)
{
	char up[32], load[32], batt[16], pid1[32], mem[64], line[256];
	FILE *f = fopen("/proc/meminfo", "r");

	(void)arg;
	field(up, sizeof(up), "/proc/uptime", 0);
	field(load, sizeof(load), "/proc/loadavg", 0);
	field(batt, sizeof(batt), "/sys/class/power_supply/battery/capacity", 0);
	field(pid1, sizeof(pid1), "/proc/1/comm", 0);
	mem[0] = 0;
	while (f && fgets(line, sizeof(line), f))
		if (sscanf(line, "MemAvailable: %63s", mem) == 1 || sscanf(line, "MemFree: %63s", mem) == 1)
			break;
	if (f)
		fclose(f);
	say(fd, "{\"agent\":\"" VERSION "\",\"stage\":\"%s\",\"uptime\":%s,\"load\":%s,\"battery\":\"%s\","
	    "\"pid1\":\"%s\",\"mem_kb\":%s}\n", access("/init_2nd.sh", F_OK) ? "rootfs" : "initramfs",
	    up, load, batt, pid1, mem[0] ? mem : "0");
}
