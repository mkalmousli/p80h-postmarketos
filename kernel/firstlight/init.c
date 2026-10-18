/* Stage-1 init: proves the mainline kernel booted by writing to the console (fbcon) forever. */
#include <stdio.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <unistd.h>

int main(void)
{
	struct utsname u;
	int n = 0;

	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
	mount("proc", "/proc", "proc", 0, NULL);
	uname(&u);
	for (;;) {
		printf("\n\n  P80H MAINLINE LINUX %s alive %d s\n", u.release, n);
		fflush(stdout);
		sleep(5);
		n += 5;
	}
}
