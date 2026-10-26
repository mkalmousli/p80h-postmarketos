/* p80h-charge: charge-only screen on the framebuffer: percentage, progress bar, ETA and touch buttons.
 * Plain black/white to be cheap to look at. Exit 0 = continue booting, never returns on power off. Battery data: any power_supply with `capacity`, else a
 * Li-ion curve from `voltage_now`; the charger's `status` always. */
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "font.h"

#define BL "/sys/class/backlight/backlight/brightness"
#define PS "/sys/class/power_supply"
#define NBTN 3
#define AUTOBOOT_MS 10000
#define SCREEN_S 30
#define CAP_MAH 3000	/* nominal pack size; only the ETA depends on it */

static unsigned char *fb, *buf;
static int fbfd;
static int W, H, stride, fbsize;
static const char *label[NBTN] = {"screen off", "power off", "boot system"};
static int by0[NBTN], by1, bx0, bx1;

static void put(int x, int y, unsigned col)
{
	if (x < 0 || y < 0 || x >= W || y >= H) return;
	*(unsigned *)(buf + y * stride + x * 4) = col;
}

static unsigned mix(unsigned a, unsigned b, int t)
{
	unsigned r = 0;
	for (int s = 0; s < 24; s += 8) r |= (((a >> s & 255) * (255 - t) + (b >> s & 255) * t) / 255) << s;
	return r;
}

static void rect(int x0, int y0, int x1, int y1, unsigned col, int rad)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) {
			int dx = x < x0 + rad ? x0 + rad - x : x >= x1 - rad ? x - (x1 - rad - 1) : 0;
			int dy = y < y0 + rad ? y0 + rad - y : y >= y1 - rad ? y - (y1 - rad - 1) : 0;
			if (dx * dx + dy * dy <= rad * rad) put(x, y, col);
		}
}

static const struct glyph *find(const struct glyph *g, int n, int c)
{
	for (int i = 0; i < n; i++) if (g[i].c == c) return &g[i];
	return NULL;
}

static int width(const char *s, int big)
{
	int w = 0;
	for (; *s; s++) {
		const struct glyph *g = big ? find(font_big, FONT_BIG_N, *s) : find(font_small, FONT_SMALL_N, *s);
		if (g) w += g->adv;
	}
	return w;
}

/* centred at cx, top at y */
static void text(int cx, int y, const char *s, unsigned col, int big)
{
	int x = cx - width(s, big) / 2;
	for (; *s; s++) {
		const struct glyph *g = big ? find(font_big, FONT_BIG_N, *s) : find(font_small, FONT_SMALL_N, *s);
		if (!g) continue;
		for (int j = 0; j < g->h; j++)
			for (int i = 0; i < g->w; i++) {
				int a = g->a[j * g->w + i];
				if (a) put(x + i, y + j, mix(*(unsigned *)(buf + (y + j) * stride + (x + i) * 4), col, a));
			}
		x += g->adv;
	}
}

static int rd(const char *path, char *out, int n)
{
	int f = open(path, O_RDONLY), r = f < 0 ? -1 : read(f, out, n - 1);
	if (f >= 0) close(f);
	if (r > 0) { out[r] = 0; out[strcspn(out, "\n")] = 0; }
	return r;
}

/* first supply that has `file`; returns -1 or the integer, copies the raw text into str */
static long supply(const char *file, char *str, int n)
{
	DIR *d = opendir(PS);
	struct dirent *e;
	char p[300];
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof p, PS "/%s/%s", e->d_name, file);
		if (rd(p, str, n) > 0) { closedir(d); return atol(str); }
	}
	if (d) closedir(d);
	return -1;
}

/* Battery gauge: MT6323 AUXADC through the PMIC wrapper (mapped from /dev/mem; the kernel has no driver for it).
 * Sequence recovered from the stock PMIC_IMM_GetOneChannelValue (p80h re): VBUF_EN (0x758 b0), request bit in
 * CON22 (0x76e: 0x80 BATSNS, 0x40 ISENSE), wait ready (bit 15 of ADC0 0x714 / ADC1 0x716), data = bits 14:0.
 * 15-bit, 1.8 V full scale, both channels divided by 4; the 68 mohm sense resistor sits between them. */
static volatile unsigned *pw;

static int pm_rd(unsigned a)
{
	int t = 100000;
	while (((pw[0xa4 / 4] >> 16) & 7) && --t);
	pw[0xa0 / 4] = (a >> 1) << 16;
	t = 100000;
	while (((pw[0xa4 / 4] >> 16) & 7) != 6 && --t);
	if (!t) return -1;
	int v = pw[0xa4 / 4] & 0xffff;
	pw[0xa8 / 4] = 1;
	return v;
}

static void pm_upd(unsigned a, unsigned mask, unsigned val)
{
	int t = 100000, v = pm_rd(a);
	if (v < 0) return;
	while (((pw[0xa4 / 4] >> 16) & 7) && --t);
	pw[0xa0 / 4] = 0x80000000u | (a >> 1) << 16 | ((v & ~mask) | (val & mask));
}

static int adc(unsigned bit, unsigned reg)
{
	pm_upd(0x76e, 0x1ff, 0);
	pm_upd(0x76e, 0x1ff, bit);
	usleep(300);
	for (int t = 0; t < 50; t++) {
		int v = pm_rd(reg);
		if (v >= 0 && (v & 0x8000)) return v & 0x7fff;
		usleep(1000);
	}
	return -1;
}

/* mV and mA (positive = charging); 0 if the PMIC did not answer */
static int gauge(int *mv, int *ma)
{
	int f, b = 0, s = 0, n = 0;
	static int mapped;
	if (!mapped) {
		f = open("/dev/mem", O_RDWR | O_SYNC);
		if (f >= 0) pw = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0x1000d000);
		mapped = 1;
	}
	if (!pw || pw == MAP_FAILED) return 0;
	pm_upd(0x758, 1, 1);
	for (int i = 0; i < 4; i++) {
		int x = adc(0x80, 0x714), y = adc(0x40, 0x716);
		if (x > 0 && y > 0) { b += x; s += y; n++; }
	}
	pm_upd(0x76e, 0x1ff, 0);
	if (!n) return 0;
	*mv = (long)b / n * 7200 / 32768;
	*ma = ((long)(s - b) / n * 7200 / 32768) * 1000 / 68;
	return 1;
}

static int soc(int ocv)	/* Li-ion open-circuit voltage to percent */
{
	static const int v[] = {3300, 3500, 3600, 3700, 3750, 3800, 3870, 3950, 4050, 4200};
	if (ocv <= v[0]) return 0;
	for (int i = 1; i < 10; i++)
		if (ocv <= v[i]) return (i - 1) * 11 + (ocv - v[i - 1]) * 11 / (v[i] - v[i - 1]) + (i == 9);
	return 100;
}

static long ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void kmsg(int code, int val)	/* key trace for `dmesg` while the power key is being sorted out */
{
	char s[64];
	int f = open("/dev/kmsg", O_WRONLY);
	if (f < 0) return;
	snprintf(s, sizeof s, "p80h-charge: key %d value %d\n", code, val);
	write(f, s, strlen(s));
	close(f);
}

static void backlight(int v)
{
	char s[16];
	int f = open(BL, O_WRONLY);
	if (f < 0) return;
	snprintf(s, sizeof s, "%d", v);
	write(f, s, strlen(s));
	close(f);
}

static void frame(int x0, int y0, int x1, int y1, unsigned col, int t)
{
	rect(x0, y0, x1, y1, col, 0);
	rect(x0 + t, y0 + t, x1 - t, y1 - t, 0, 0);
}

static void cores(int on)	/* nothing heavy while charging: one core */
{
	char p[64];
	for (int i = 1; i < 4; i++) {
		snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/online", i);
		int f = open(p, O_WRONLY);
		if (f >= 0) { write(f, on ? "1" : "0", 1); close(f); }
	}
}

/* plain on purpose: black, white, monospace; the full screen is redrawn on change, only the countdown line per tick */
#define BARY (1240 * H / 1280)

static void flush(int y0, int y1)
{
	struct fb_var_screeninfo v;
	memcpy(fb + y0 * stride, buf + y0 * stride, (y1 - y0) * stride);
	if (!ioctl(fbfd, FBIOGET_VSCREENINFO, &v)) { v.yoffset = 0; ioctl(fbfd, FBIOPAN_DISPLAY, &v); }
}

static void countdown(long left_ms)	/* thin line draining to the right; no text */
{
	int u = W / 800 ? W / 800 : 1, w = W - 200 * u, x = 100 * u;
	rect(0, BARY, W, BARY + 6 * u, 0, 0);
	rect(x, BARY, x + (long)w * left_ms / (SCREEN_S * 1000), BARY + 4 * u, 0x00707070, 0);
	flush(BARY, BARY + 6 * u);
}

static void draw(int pct, const char *status, int eta_min, int sel, const char *detail)
{
	char s[64];
	int u = W / 800 ? W / 800 : 1;
	memset(buf, 0, fbsize);
	{
		time_t now = time(NULL);
		strftime(s, sizeof s, "%a %d %b   %H:%M", localtime(&now));
		text(W / 2, 40 * H / 1280, s, 0x00a0a0a0, 0);
	}
	if (pct >= 0) snprintf(s, sizeof s, "%d%%", pct); else snprintf(s, sizeof s, " ");
	text(W / 2, 100 * H / 1280, s, 0x00ffffff, 1);
	int bx = 100 * u, bw = W - 200 * u, by = 340 * H / 1280, bh = 26 * u;
	frame(bx, by, bx + bw, by + bh, 0x00ffffff, 2 * u);
	if (pct > 0) rect(bx + 5 * u, by + 5 * u, bx + 5 * u + (bw - 10 * u) * pct / 100, by + bh - 5 * u, 0x00ffffff, 0);
	snprintf(s, sizeof s, "%s%s%s", status, detail[0] ? "   " : "", detail);
	text(W / 2, by + bh + 22 * u, s, 0x00ffffff, 0);
	if (eta_min > 0) snprintf(s, sizeof s, eta_min >= 60 ? "full in %dh %02dm" : "full in %d min", eta_min >= 60 ? eta_min / 60 : eta_min, eta_min % 60);
	else s[0] = 0;
	text(W / 2, by + bh + 66 * u, s, 0x00a0a0a0, 0);
	bx0 = 140 * u; bx1 = W - 140 * u;
	for (int i = 0; i < NBTN; i++) {
		by0[i] = 520 * H / 1280 + i * 84 * u;
		by1 = 68 * u;
		frame(bx0, by0[i], bx1, by0[i] + by1, i == sel ? 0x00ffffff : 0x00606060, 2 * u);
		if (i == sel) rect(bx0, by0[i], bx1, by0[i] + by1, 0x00ffffff, 0);
		text(W / 2, by0[i] + 18 * u, label[i], i == sel ? 0 : 0x00c0c0c0, 0);
	}
	text(W / 2, 800 * H / 1280, "vol+ move   vol- ok   pwr screen", 0x00606060, 0);
	flush(0, H);
}

/* the pmOS init enters its debug shell while Volume-Down (or Up) is held: let go of every key before booting */
static void wait_release(struct pollfd *p, int n)
{
	for (int tries = 0; tries < 500; tries++) {	/* 5 s at most */
		int held = 0;
		for (int i = 0; i < n; i++) {
			unsigned char k[KEY_MAX / 8 + 1] = {0};
			if (ioctl(p[i].fd, EVIOCGKEY(sizeof k), k) < 0) continue;
			for (int c = KEY_VOLUMEDOWN; c <= KEY_POWER; c++) held |= k[c / 8] & (1 << (c % 8));
		}
		if (!held) return;
		usleep(10000);
	}
}

int main(void)
{
	int f = open("/dev/fb0", O_RDWR);
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo fx;
	if (f < 0 || ioctl(f, FBIOGET_VSCREENINFO, &v) || ioctl(f, FBIOGET_FSCREENINFO, &fx) || v.bits_per_pixel != 32) return 1;
	fbfd = f;
	W = v.xres; H = v.yres; stride = fx.line_length; fbsize = stride * H;
	fb = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
	buf = malloc(fbsize);
	if (fb == MAP_FAILED || !buf) return 1;

	struct pollfd p[8];
	int n = 0;
	for (int i = 0; i < 8; i++) {
		char path[32];
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		int e = open(path, O_RDONLY | O_NONBLOCK);
		if (e < 0) continue;
		struct input_absinfo a;
		if (!ioctl(e, EVIOCGABS(ABS_MT_POSITION_X), &a)) { close(e); continue; }	/* touch is off: only keys wake the menu */
		p[n].fd = e; p[n++].events = POLLIN;
	}

	char st[64], cur[16];
	rd(BL, cur, sizeof cur);
	long last[3] = {0};	/* KEY_VOLUMEDOWN 114, KEY_VOLUMEUP 115, KEY_POWER 116: ignore bounces within 300 ms */
	int sel = -1, bright = atoi(cur) ? atoi(cur) : 128, off = 0, mv = 0, ma = 0, pct = -1, eta = 0;
	long idle = ms(), lastg = -100000;
	int redraw = 0;
	long picked = 0, start = ms();
	int touched = 0;	/* no key within AUTOBOOT_MS: boot the system by itself */
	cores(0);
	{ int g = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", O_WRONLY); if (g >= 0) { write(g, "powersave", 9); close(g); } }
	backlight(24);
	{ int f = open(PS "/bq24156-0/current_limit", O_WRONLY); if (f >= 0) { write(f, "800", 3); close(f); } }	/* fastest input limit the FAN5405 has below "none" */
	for (;;) {
		long now = ms();
		int dirty = 0;
		if (!touched && now - start > AUTOBOOT_MS) { wait_release(p, n); cores(1); backlight(bright); return 0; }
		if (!off && now - lastg > 5000) {
			lastg = now;
			dirty = 1;
			supply("status", st, sizeof st);
			if (gauge(&mv, &ma)) {
				pct = soc(mv - (ma > 0 ? ma * 150 / 1000 : 0));	/* terminal voltage minus ~150 mohm drop while charging */
				if (!strcmp(st, "Full")) pct = 100;
				eta = (pct < 100 && ma > 60) ? (int)((100 - pct) * CAP_MAH / 100 * 60 / ma * 125 / 100) : 0;	/* +25 % for the constant-voltage tail */
			}
		}
		if (sel >= 0 && now - picked > 6000) { sel = -1; redraw = 1; }	/* highlight times out */
		if (!off && (dirty || redraw)) {
			char detail[48] = "";
			if (mv) snprintf(detail, sizeof detail, "%d.%02d V  %+d mA", mv / 1000, mv / 10 % 100, ma);
			draw(pct, st[0] ? st : "Unknown", eta, sel, detail);
			redraw = 0;
		}
		if (!off) {
			long left = SCREEN_S * 1000 - (now - idle);
			if (left <= 0) { off = 1; backlight(0); continue; }
			countdown(left);
		}
		if (poll(p, n, off ? 60000 : 250) <= 0) continue;
		int act = 0, next = 0, pwr = 0;
		for (int i = 0; i < n; i++) {
			struct input_event ev;
			while (read(p[i].fd, &ev, sizeof ev) == sizeof ev) {
				if (ev.type != EV_KEY) continue;
				kmsg(ev.code, ev.value);
				if (ev.code < KEY_VOLUMEDOWN || ev.code > KEY_POWER) continue;
				int k = ev.code - KEY_VOLUMEDOWN, want = ev.code == KEY_POWER ? 0 : 1;	/* volume on press, power on release: a click */
				if (ev.value != want || ms() - last[k] < 300) continue;
				last[k] = ms();
				if (ev.code == KEY_VOLUMEUP) next = 1;
				else if (ev.code == KEY_VOLUMEDOWN) act = 1;
				else pwr = 1;
			}
		}
		if (off) {	/* only the power key wakes it */
			if (pwr) { off = 0; idle = ms(); lastg = -100000; backlight(24); }
			continue;
		}
		if (!act && !next && !pwr) continue;
		touched = 1;
		idle = ms();
		redraw = 1;
		if (pwr) { off = 1; backlight(0); continue; }	/* power click: screen off */
		if (next) { sel = (sel + 1) % NBTN; picked = ms(); continue; }
		if (act && sel < 0) continue;	/* nothing highlighted: volume keys do nothing */
		if (sel == 2) { wait_release(p, n); usleep(200000); cores(1); backlight(bright); return 0; }
		if (sel == 0) { off = 1; backlight(0); }
		if (sel == 1) {	/* no cable-proof power off on this board: if the PMIC does not cut us, come back via reset */
			sync();
			if (!fork()) { sleep(8); int s = open("/proc/sysrq-trigger", O_WRONLY); write(s, "b", 1); _exit(0); }
			int s = open("/proc/sysrq-trigger", O_WRONLY);
			write(s, "o", 1);
			for (;;) pause();
		}
	}
}
