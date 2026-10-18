/* SHELL <rows> <cols>: interactive /bin/sh on a pty, raw bytes both ways until either side closes. */
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "p80hd.h"

static void relay(int a, int b)
{
	struct pollfd p[2] = { { .fd = a, .events = POLLIN }, { .fd = b, .events = POLLIN } };
	char buf[4096];

	while (poll(p, 2, -1) > 0) {
		for (int i = 0; i < 2; i++) {
			if (!(p[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;
			long n = read(p[i].fd, buf, sizeof(buf));
			if (n <= 0 || write_full(p[1 - i].fd, buf, n))
				return;
		}
	}
}

void cmd_shell(int fd, const char *arg)
{
	struct winsize ws = { .ws_row = 24, .ws_col = 80 };
	int m = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	pid_t p;

	sscanf(arg, "%hu %hu", &ws.ws_row, &ws.ws_col);
	if (m < 0 || grantpt(m) || unlockpt(m)) {
		say(fd, "ERR no pty (devpts not mounted?)\n");
		return;
	}
	ioctl(m, TIOCSWINSZ, &ws);
	if (!(p = fork())) {
		int s = open(ptsname(m), O_RDWR);

		setsid();
		ioctl(s, TIOCSCTTY, 0);
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		for (int i = 3; i < 64; i++)
			close(i);
		setenv("TERM", "xterm", 1);
		setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
		execl("/bin/sh", "-sh", (char *)0);
		_exit(127);
	}
	say(fd, "OK\n");
	relay(fd, m);
	kill(p, SIGHUP);
	waitpid(p, 0, 0);
}
