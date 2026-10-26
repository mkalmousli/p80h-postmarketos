/* PUT <path> <size> [mode]: atomic write (tmp + rename; block devices written in place).
 * GET <path> [offset [len]]: answers "<size>\n" + bytes. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "p80hd.h"

void cmd_put(int fd, const char *arg)
{
	char path[1024], tmp[1100], buf[65536];
	long size, n;
	unsigned mode = 0644;
	long total;
	int dev, f, pct = -1, last = -1;
	char label[160];
	const char *name;

	if (sscanf(arg, "%1023s %ld %o", path, &size, &mode) < 2) {
		say(fd, "ERR usage\n");
		return;
	}
	/* never create files under /dev: a missing block node must fail, not become a regular file */
	dev = !strncmp(path, "/dev/", 5);
	snprintf(tmp, sizeof(tmp), "%s.p80h-tmp", path);
	f = open(dev ? path : tmp, dev ? O_WRONLY : O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (f < 0) {
		say(fd, "ERR open\n");
		return;
	}
	if (!dev)
		fchmod(f, mode);  /* open() applies the mode only on create, and through the umask */
	name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
	snprintf(label, sizeof(label), "%s %.28s", dev ? "WRITING" : "RECEIVING", name);
	total = size;
	for (; size > 0; size -= n) {
		pct = total >= (1 << 20) ? (int)((total - size) * 100 / total) : -2;	/* only big transfers get a screen */
		if (pct != last && pct != -2)
			progress(label, last = pct);
		n = read(fd, buf, size < (long)sizeof(buf) ? size : (long)sizeof(buf));
		if (n <= 0 || write_full(f, buf, n))
			break;
	}
	fsync(f);
	close(f);
	if (last >= 0)
		progress("", 0);
	if (!size && !dev)
		size = rename(tmp, path);
	else if (!dev)
		unlink(tmp);
	say(fd, size ? "ERR short\n" : "OK\n");
}

void cmd_get(int fd, const char *arg)
{
	char buf[65536], path[1024];
	long off = 0, len = -1, n;
	struct stat st;
	int f;

	sscanf(arg, "%1023s %ld %ld", path, &off, &len);
	f = open(path, O_RDONLY);
	if (f < 0 || fstat(f, &st)) {
		say(fd, "ERR open\n");
		return;
	}
	if (!S_ISREG(st.st_mode) || len >= 0) /* char/block devices report size 0: caller gives the length */
		st.st_size = len >= 0 ? len : 0;
	else
		st.st_size -= off;
	say(fd, "%ld\n", (long)st.st_size);
	lseek(f, off, SEEK_SET);
	for (long left = st.st_size; left > 0; left -= n) {
		n = read(f, buf, left < (long)sizeof(buf) ? left : (long)sizeof(buf));
		if (n <= 0 || write_full(fd, buf, n))
			break;
	}
	close(f);
}
