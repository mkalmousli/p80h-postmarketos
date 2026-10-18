/* EXEC [secs] <shell command>: run in the *current* root, stream output, end with "@@EXIT n" or "@@TIMEOUT". */
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "p80hd.h"

static int procfd = -1;

/* Keep /proc open from boot: initramfs moves its mounts at switch_root, a path lookup would then miss it. */
void exec_init(void)
{
	procfd = open("/proc", O_RDONLY | O_DIRECTORY);
}

/* The daemon starts in the initramfs; after switch_root its own root is a dead tree, PID 1's is the real one. */
void enter_root(void)
{
	if (procfd >= 0 && fchdir(procfd) == 0 && chdir("1/root") == 0 && chroot(".") == 0 && chdir("/") == 0)
		return;
}

void cmd_exec(int fd, const char *arg)
{
	char *end;
	long secs = isdigit((unsigned char)*arg) ? strtol(arg, &end, 10) : 0;
	int st = 0, timed_out = 0;
	pid_t p;

	if (secs)
		arg = end + (*end == ' ');
	p = fork();
	if (!p) {
		setpgid(0, 0);
		signal(SIGPIPE, SIG_DFL);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd); /* else background children keep the connection open */
		close(0);
		setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
		execl("/bin/sh", "sh", "-c", arg, (char *)0);
		_exit(127);
	}
	for (time_t t0 = time(0); waitpid(p, &st, WNOHANG) != p; usleep(10000)) {
		if (secs && time(0) - t0 >= secs) {
			kill(-p, SIGKILL);
			waitpid(p, &st, 0);
			timed_out = 1;
			break;
		}
	}
	if (timed_out)
		say(fd, "\n@@TIMEOUT\n");
	else
		say(fd, "\n@@EXIT %d\n", WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st));
}
