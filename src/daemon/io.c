/* Socket and file helpers shared by all handlers. */
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <unistd.h>
#include "p80hd.h"

int listen_on(const char *addr, int port)
{
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
	int one = 1, s = socket(AF_INET, SOCK_STREAM, 0);

	inet_pton(AF_INET, addr, &a.sin_addr);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	setsockopt(s, IPPROTO_IP, IP_FREEBIND, &one, sizeof(one)); /* bind before the USB link exists */
	if (bind(s, (void *)&a, sizeof(a)) || listen(s, 8))
		return -1;
	return s;
}

void tune(int fd)
{
	struct timeval t = { .tv_sec = 120 };
	int one = 1, idle = 10;

	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
}

int read_line(int fd, char *line, int max, int secs)
{
	struct pollfd p = { .fd = fd, .events = POLLIN };

	for (int i = 0; i < max - 1; i++) {
		if (poll(&p, 1, secs * 1000) != 1 || read(fd, line + i, 1) != 1)
			return -1;
		if (line[i] == '\n') {
			line[i] = 0;
			return 0;
		}
	}
	return -1;
}

int write_full(int fd, const void *buf, long n)
{
	const char *p = buf;

	while (n > 0) {
		long w = write(fd, p, n);
		if (w <= 0)
			return -1;
		p += w;
		n -= w;
	}
	return 0;
}

int read_full(int fd, void *buf, long n)
{
	char *p = buf;

	while (n > 0) {
		long r = read(fd, p, n);
		if (r <= 0)
			return -1;
		p += r;
		n -= r;
	}
	return 0;
}

int say(int fd, const char *fmt, ...)
{
	char b[1024];
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	return write_full(fd, b, n);
}

long slurp(const char *path, char *buf, long max)
{
	int f = open(path, O_RDONLY);
	long n = f < 0 ? -1 : read(f, buf, max - 1);

	if (f >= 0)
		close(f);
	buf[n > 0 ? n : 0] = 0;
	return n;
}
