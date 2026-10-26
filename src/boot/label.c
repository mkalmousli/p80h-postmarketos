/* progress messages from p80hd and the text drawing */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "font.h"
#include "splash.h"

static int sock = -1;
const float acc[3] = {76, 195, 255};

int ui_open(void)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	memcpy(a.sun_path + 1, "p80h-ui", 7);	/* abstract: survives switch_root, no files */
	sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	return sock < 0 || bind(sock, (void *)&a, offsetof(struct sockaddr_un, sun_path) + 8);
}

int ui_poll(char *label, int n, int *pm)
{
	char b[160], *t;
	ssize_t r = sock < 0 ? -1 : recv(sock, b, sizeof(b) - 1, 0);
	if (r < 0) return 0;
	b[r] = 0;
	t = strchr(b, '\t');
	*pm = t ? atoi(t + 1) : -1;
	if (t) *t = 0;
	snprintf(label, n, "%s", b);
	return 1;
}

static unsigned blend(unsigned fg, unsigned bg, int a)
{
	unsigned o = 0;
	for (int s = 0; s < 24; s += 8) o |= (((fg >> s & 255) * a + (bg >> s & 255) * (255 - a)) / 255) << s;
	return o;
}

void ui_text(unsigned char *fb, int stride, int x, int y, const char *s, unsigned fg, unsigned bg, int align)
{
	int n = (int)strlen(s);
	x -= align < 0 ? 0 : align > 0 ? n * GW : n * GW / 2;
	for (; *s; s++, x += GW)
		for (int j = 0; j < GH; j++)
			for (int i = 0; i < GW; i++) {
				int a = font[*s < 32 || *s > 126 ? 0 : *s - 32][j * GW + i];
				if (a) px(fb, stride, x + i, y + j, blend(fg, bg, a));
			}
}
