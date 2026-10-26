/*
 * p80h-clock drawing. The logical canvas is landscape LW x LH; blit() turns it 90 degrees clockwise onto the
 * 800x1280 portrait panel. Strokes are distance-field capsules: a sharp core of radius r and an additive glow g.
 * Digits are rounded seven-segment paths (Chaikin with a small cut, resampled to N points) so they can morph.
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "font.h"
#include "gfx.h"

#define CUT .12f	/* corner cut per Chaikin pass */

unsigned *bg, *cv;
float pal_core[3], pal_glow[3], pal_bg[3];
static unsigned char *fb;
static int stride;
static float *dist;
static pt shape[11][N];	/* 0-9 and 10 = dash (the boot frame) */

unsigned rgb(const float *c, float k)
{
	int r = c[0] * k, g = c[1] * k, b = c[2] * k;
	return (r > 255 ? 255 : r < 0 ? 0 : r) << 16 | (g > 255 ? 255 : g < 0 ? 0 : g) << 8 | (b > 255 ? 255 : b < 0 ? 0 : b);
}

unsigned add(unsigned a, unsigned b, int t)
{
	unsigned r = 0;
	for (int s = 0; s < 24; s += 8) {
		unsigned v = (a >> s & 255) + (b >> s & 255) * t / 255;
		r |= (v > 255 ? 255 : v) << s;
	}
	return r;
}

unsigned mix(unsigned a, unsigned b, int t)
{
	unsigned r = 0;
	t = t < 0 ? 0 : t > 255 ? 255 : t;
	for (int s = 0; s < 24; s += 8) r |= (((a >> s & 255) * (255 - t) + (b >> s & 255) * t) / 255) << s;
	return r;
}

/* palette keyframes over the day: hour, core rgb (teal night, lime dawn, green day, aqua dusk) */
static const float keys[][4] = {
	{0, 40, 200, 160}, {5, 50, 180, 120}, {7, 190, 255, 110}, {9, 120, 255, 120}, {12, 70, 255, 130},
	{17, 110, 255, 160}, {19.5f, 80, 235, 215}, {22, 50, 205, 170}, {24, 40, 200, 160},
};

void palette(float hour)
{
	int i = 0;
	while (keys[i + 1][0] < hour) i++;
	float t = (hour - keys[i][0]) / (keys[i + 1][0] - keys[i][0]);
	for (int c = 0; c < 3; c++) {
		pal_core[c] = keys[i][c + 1] + (keys[i + 1][c + 1] - keys[i][c + 1]) * t;
		pal_glow[c] = pal_core[c] * 0.42f;
		pal_bg[c] = pal_core[c] * 0.11f;
	}
}

void background(void)	/* radial falloff in the day's tint, every third line darker (scanlines) */
{
	for (int y = 0; y < LH; y++)
		for (int x = 0; x < LW; x++) {
			float dx = (x - LW / 2) / (float)(LW / 2), dy = (y - LH / 2) / (float)(LH / 2);
			float d = dx * dx * 0.8f + dy * dy, k = d > 1.25f ? 0 : 1 - d / 1.25f;
			k = k * k * (3 - 2 * k) + 0.06f;
			if (y % 3 == 2) k *= 0.72f;
			bg[y * LW + x] = rgb(pal_bg, k);
		}
}

/* seven-segment nodes: 0 TL 1 TR 2 ML 3 MR 4 BL 5 BR 6 top middle 7 bottom middle 8 flag of the 1;
 * each digit is one stroke (retraces allowed) */
static const float node[9][2] = {{0, 0}, {1, 0}, {0, .5f}, {1, .5f}, {0, 1}, {1, 1}, {.5f, 0}, {.5f, 1}, {.18f, .2f}};
static const char *route[11] = {"615406", "867", "013245", "0132354", "02315", "102354", "104532", "015", "2310453", "451023", "23"};

static void build_shape(int d)
{
	static pt p[1024], q[1024];
	static float len[1024];
	int n = 0;
	for (const char *r = route[d]; *r; r++) p[n++] = (pt){node[*r - '0'][0], node[*r - '0'][1]};
	for (int it = 0; it < 4; it++) {	/* Chaikin with a small cut: rounded but crisp corners, keeps the ends */
		int m = 0;
		q[m++] = p[0];
		for (int i = 0; i + 1 < n; i++) {
			pt a = p[i], b = p[i + 1];
			if (i > 0) q[m++] = (pt){a.x * (1 - CUT) + b.x * CUT, a.y * (1 - CUT) + b.y * CUT};
			if (i + 2 < n) q[m++] = (pt){a.x * CUT + b.x * (1 - CUT), a.y * CUT + b.y * (1 - CUT)};
		}
		q[m++] = p[n - 1];
		memcpy(p, q, m * sizeof(pt));
		n = m;
	}
	float total = 0;	/* resample to N points evenly along the path (x weighted by the cell aspect) */
	len[0] = 0;
	for (int i = 1; i < n; i++) len[i] = total += hypotf((p[i].x - p[i - 1].x) * .55f, p[i].y - p[i - 1].y);
	for (int j = 0, i = 1; j < N; j++) {
		float s = total * j / (N - 1);
		while (i < n - 1 && len[i] < s) i++;
		float t = len[i] > len[i - 1] ? (s - len[i - 1]) / (len[i] - len[i - 1]) : 0;
		shape[d][j] = (pt){p[i - 1].x + (p[i].x - p[i - 1].x) * t, p[i - 1].y + (p[i].y - p[i - 1].y) * t};
	}
}

static int alloc(void)
{
	bg = malloc(LW * LH * 4);
	cv = malloc(LW * LH * 4);
	dist = malloc(LW * LH * sizeof(float));
	for (int d = 0; d < 11; d++) build_shape(d);
	return bg && cv && dist ? 0 : -1;
}

int gfx_mem(unsigned char *panel, int panel_stride)	/* render into memory (bootframe.c, on the PC) */
{
	fb = panel;
	stride = panel_stride;
	return alloc();
}

int gfx_open(void)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo fx;
	int fd = -1;
	for (int i = 0; i < 50 && (fd = open("/dev/fb0", O_RDWR)) < 0; i++) usleep(100000);
	if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &fx) ||
	    v.bits_per_pixel != 32 || v.xres != LH || v.yres != LW)
		return -1;
	stride = fx.line_length;
	fb = mmap(NULL, stride * v.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED || alloc()) return -1;
	v.yoffset = 0;
	ioctl(fd, FBIOPAN_DISPLAY, &v);
	return 0;
}

static void clip(int *x0, int *y0, int *x1, int *y1)
{
	if (*x0 < 0) *x0 = 0;
	if (*y0 < 0) *y0 = 0;
	if (*x1 > LW) *x1 = LW;
	if (*y1 > LH) *y1 = LH;
}

void restore(int x0, int y0, int x1, int y1)
{
	clip(&x0, &y0, &x1, &y1);
	for (int y = y0; y < y1; y++) memcpy(&cv[y * LW + x0], &bg[y * LW + x0], (x1 - x0) * 4);
}

void blit(int x0, int y0, int x1, int y1)	/* logical rect -> portrait panel, turned 90 degrees clockwise */
{
	clip(&x0, &y0, &x1, &y1);
	for (int x = x0; x < x1; x++) {
		unsigned *row = (unsigned *)(fb + x * stride);
		for (int y = y0; y < y1; y++) row[LH - 1 - y] = cv[y * LW + x];
	}
}

void panel_fill(int x0, int y0, int x1, int y1, unsigned col)
{
	clip(&x0, &y0, &x1, &y1);
	for (int x = x0; x < x1; x++) {
		unsigned *row = (unsigned *)(fb + x * stride);
		for (int y = y0; y < y1; y++) row[LH - 1 - y] = col;
	}
}

void dim(int x0, int y0, int x1, int y1, int keep)
{
	clip(&x0, &y0, &x1, &y1);
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) cv[y * LW + x] = mix(0, cv[y * LW + x], keep);
}

void fill(int x0, int y0, int x1, int y1, unsigned col)
{
	clip(&x0, &y0, &x1, &y1);
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) cv[y * LW + x] = col;
}

void stroke(const cellbox *c, const pt *p, int n, float boost)
{
	int W = c->w, H = c->h;
	float reach = c->r + c->g;
	for (int i = 0; i < W * H; i++) dist[i] = 1e9f;
	for (int i = 0; i < (n > 1 ? n - 1 : 1); i++) {
		pt a = p[i], b = p[n > 1 ? i + 1 : i];
		int x0 = fmaxf(0, fminf(a.x, b.x) - reach), x1 = fminf(W, fmaxf(a.x, b.x) + reach + 1);
		int y0 = fmaxf(0, fminf(a.y, b.y) - reach), y1 = fminf(H, fmaxf(a.y, b.y) + reach + 1);
		float vx = b.x - a.x, vy = b.y - a.y, vv = vx * vx + vy * vy;
		for (int y = y0; y < y1; y++)
			for (int x = x0; x < x1; x++) {
				float wx = x - a.x, wy = y - a.y, t = vv > 0 ? (wx * vx + wy * vy) / vv : 0;
				t = t < 0 ? 0 : t > 1 ? 1 : t;
				float dx = wx - vx * t, dy = wy - vy * t, d = dx * dx + dy * dy;
				if (d < dist[y * W + x]) dist[y * W + x] = d;
			}
	}
	unsigned core = rgb(pal_core, 1), glow = rgb(pal_glow, 1 + boost);
	for (int y = 0; y < H; y++) {
		int cy = c->y + y;
		if (cy < 0 || cy >= LH) continue;
		for (int x = 0; x < W; x++) {
			int cx = c->x + x;
			float d2 = dist[y * W + x];
			if (d2 >= reach * reach || cx < 0 || cx >= LW) continue;
			float d = sqrtf(d2);
			unsigned *px = &cv[cy * LW + cx];
			if (d > c->r - 1) {
				float k = 1 - (d - c->r + 1) / (c->g + 1);
				*px = add(*px, glow, k * k * 230);
			}
			if (d < c->r + 1) *px = mix(*px, core, d < c->r - 1 ? 255 : (c->r + 1 - d) * 127);
		}
	}
}

void line(const cellbox *c, float x0, float y0, float x1, float y1, float boost)
{
	pt p[2] = {{x0, y0}, {x1, y1}};
	stroke(c, p, 2, boost);
}

void digit(const cellbox *c, int a, int b, float t, float boost)
{
	pt p[N];
	float m = c->r + c->g, w = c->w - 2 * m, h = c->h - 2 * m, e = t * t * t * (t * (6 * t - 15) + 10);	/* smootherstep */
	for (int i = 0; i < N; i++) {
		float x = shape[a][i].x + (shape[b][i].x - shape[a][i].x) * e;
		float y = shape[a][i].y + (shape[b][i].y - shape[a][i].y) * e;
		p[i] = (pt){m + x * w, m + y * h};
	}
	if (t < 1) {	/* in motion: lean glow (4x cheaper splats) brightening towards the end; full glow lands on t = 1 */
		cellbox lean = *c;
		lean.g = 8;
		stroke(&lean, p, N, boost * e);
	} else stroke(c, p, N, 0);
}

static const struct glyph *set(enum font f, int *n)
{
	switch (f) {
	case F_BAT: *n = FONT_BAT_N; return font_bat;
	default: *n = FONT_DATE_N; return font_date;
	}
}

static const struct glyph *find(const struct glyph *g, int n, int c)
{
	for (int i = 0; i < n; i++) if (g[i].c == c) return &g[i];
	return NULL;
}

int text_w(enum font f, const char *s)
{
	int n, w = 0;
	const struct glyph *g0 = set(f, &n);
	for (; *s; s++) { const struct glyph *g = find(g0, n, *s); if (g) w += g->adv; }
	return w;
}

void text(enum font f, int x, int y, const char *s, unsigned col)
{
	int n;
	const struct glyph *g0 = set(f, &n);
	for (; *s; s++) {
		const struct glyph *g = find(g0, n, *s);
		if (!g) continue;
		for (int j = 0; j < g->h; j++)
			for (int i = 0; i < g->w; i++) {
				int px = x - g->hpad + i, py = y + j;
				if (px >= 0 && py >= 0 && px < LW && py < LH && g->a[j * g->w + i])
					cv[py * LW + px] = mix(cv[py * LW + px], col, g->a[j * g->w + i]);
			}
		x += g->adv;
	}
}

void text_c(enum font f, int cx, int y, const char *s, unsigned col)
{
	text(f, cx - text_w(f, s) / 2, y, s, col);
}
