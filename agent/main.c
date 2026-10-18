/* Supervisor + accept loop. The parent only keeps the listening socket and restarts the worker if it dies. */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include "p80hd.h"

static void serve(int fd)
{
	static const struct { const char *name; void (*fn)(int, const char *); } cmds[] = {
		{ "EXEC", cmd_exec }, { "SHELL", cmd_shell }, { "PUT", cmd_put }, { "GET", cmd_get },
		{ "REBOOT", cmd_reboot }, { "INJECT", cmd_inject }, { "STATUS", cmd_status },
	};
	char line[4096];

	enter_root();
	if (read_line(fd, line, sizeof(line), 10))
		return;
	if (!strcmp(line, "PING")) {
		say(fd, "p80hd " VERSION "\n");
		return;
	}
	for (unsigned i = 0; i < sizeof(cmds) / sizeof(*cmds); i++) {
		size_t n = strlen(cmds[i].name);
		if (!strncmp(line, cmds[i].name, n) && (line[n] == ' ' || !line[n])) {
			cmds[i].fn(fd, line[n] ? line + n + 1 : "");
			return;
		}
	}
	say(fd, "ERR unknown command\n");
}

static void worker(int s)
{
	struct pollfd p = { .fd = s, .events = POLLIN };
	int live = 0;

	for (;;) {
		while (waitpid(-1, 0, WNOHANG) > 0)
			live--;
		if (poll(&p, 1, 1000) != 1)
			continue;
		int c = accept(s, 0, 0);
		if (c < 0)
			continue;
		if (live >= MAX_CONN) {
			say(c, "ERR busy\n");
		} else if (!fork()) {
			close(s);
			tune(c);
			serve(c);
			_exit(0);
		} else {
			live++;
		}
		close(c);
	}
}

int main(int argc, char **argv)
{
	const char *addr = DEFAULT_ADDR;
	int port = DEFAULT_PORT, s, o;

	while ((o = getopt(argc, argv, "b:p:")) != -1) {
		if (o == 'b')
			addr = optarg;
		else if (o == 'p')
			port = atoi(optarg);
	}
	exec_init();
	signal(SIGPIPE, SIG_IGN);
	if ((s = listen_on(addr, port)) < 0)
		return 1;
	for (;;) {
		pid_t w = fork();

		if (!w) {
			worker(s);
			_exit(1);
		}
		waitpid(w, 0, 0);
		sleep(1); /* worker crashed: the socket stays open, a new worker takes over */
	}
}
