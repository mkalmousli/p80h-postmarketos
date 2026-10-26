/* progress messages from p80hd and the label text under the ring */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "font.h"
#include "splash.h"

#define MAXCH 40	/* characters per label */
static int sock = -1;
const float acc[3] = {76, 195, 255};

int ui_open(void)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	memcpy(a.sun_path + 1, "p80h-ui", 7);	/* abstract: survives switch_root, no files */
	sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	return sock < 0 || bind(sock, (void *)&a, offsetof(struct sockaddr_un, sun_path) + 8);
}

int ui_poll(char *label, int n, int *pct)
{
	char b[160], *t;
	ssize_t r = sock < 0 ? -1 : recv(sock, b, sizeof(b) - 1, 0);
	if (r < 0) return 0;
	b[r] = 0;
	t = strchr(b, '\t');
	*pct = t ? atoi(t + 1) : -1;
	if (t) *t = 0;
	snprintf(label, n, "%s", b);
	return 1;
}

static void put(unsigned char *fb, int stride, int x, int y, unsigned px) { ((unsigned *)(fb + x * stride))[LH - 1 - y] = px; }

void ui_clear(unsigned char *fb, int stride, int cx, int y)
{
	for (int j = 0; j < GH; j++)
		for (int i = -MAXCH * GW / 2; i < MAXCH * GW / 2; i++) put(fb, stride, cx + i, y + j, 0);
}

void ui_text(unsigned char *fb, int stride, int cx, int y, const char *s, float k)
{
	int x = cx - (int)strnlen(s, MAXCH) * GW / 2;
	for (int c = 0; *s && c < MAXCH; s++, c++, x += GW)
		for (int j = 0; j < GH; j++)
			for (int i = 0; i < GW; i++) {
				float a = font[*s < 32 || *s > 126 ? 0 : *s - 32][j * GW + i] / 255.f * k * .8f;
				put(fb, stride, x + i, y + j, (unsigned)(acc[0] * a) << 16 | (unsigned)(acc[1] * a) << 8 | (unsigned)(acc[2] * a));
			}
}
