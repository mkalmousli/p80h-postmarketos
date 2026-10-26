/*
 * p80h-clock interaction. Keys: power (short: display off/on, long 0.7 s: quick actions), volume up/down
 * (brightness; in menus: move / change, with auto-repeat). Touch: taps, mapped from the portrait Silead panel to
 * the landscape canvas. Quick actions: set time & date, reboot, power off, close. The time goes to the system
 * clock and the MT6323 RTC (UTC), so it survives power-off.
 */
#include <fcntl.h>
#include <linux/input.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "gfx.h"
#include "ui.h"

#define BL "/sys/class/backlight/backlight/brightness"
#define LONG_MS 700
#define REPEAT_MS 130
#define FRAME 33

/* ---------- input ---------- */
static struct pollfd fds[8];
static int nfds, touch_fd = -1, tmax_x = 800, tmax_y = 1280;
static long power_down, rep_next;
static int power_long_sent, rep_key;
static struct uiev queue[16];
static int qhead, qtail;
static int touch_id = -1, tx, ty, tx0, ty0;
static long touch_t0;

long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000;
}

static int has(int fd, int type, int code)
{
	unsigned char bits[KEY_MAX / 8 + 1] = {0};
	return ioctl(fd, EVIOCGBIT(type, sizeof bits), bits) >= 0 && bits[code / 8] & (1 << (code % 8));
}

void input_open(void)
{
	char path[32];
	for (int i = 0; i < 8; i++) {
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		int e = open(path, O_RDONLY | O_NONBLOCK);
		if (e < 0) continue;
		int keys = has(e, EV_KEY, KEY_POWER) || has(e, EV_KEY, KEY_VOLUMEUP) || has(e, EV_KEY, KEY_VOLUMEDOWN);
		int touch = has(e, EV_ABS, ABS_MT_POSITION_X);
		if (!keys && !touch) { close(e); continue; }
		if (touch) {
			struct input_absinfo a;
			if (!ioctl(e, EVIOCGABS(ABS_MT_POSITION_X), &a)) tmax_x = a.maximum;
			if (!ioctl(e, EVIOCGABS(ABS_MT_POSITION_Y), &a)) tmax_y = a.maximum;
			touch_fd = e;
		}
		fds[nfds].fd = e;
		fds[nfds++].events = POLLIN;
	}
}

static void push(int type, int x, int y)
{
	queue[qtail] = (struct uiev){type, x, y};
	qtail = (qtail + 1) % 16;
}

static void feed(int fd, struct input_event *ev, long t)
{
	if (ev->type == EV_KEY && ev->code == KEY_POWER) {
		if (ev->value == 1) { power_down = t; power_long_sent = 0; }
		else if (ev->value == 0 && power_down) {
			if (!power_long_sent) push(E_POWER, 0, 0);
			power_down = 0;
		}
	} else if (ev->type == EV_KEY && (ev->code == KEY_VOLUMEUP || ev->code == KEY_VOLUMEDOWN)) {
		int k = ev->code == KEY_VOLUMEUP ? E_UP : E_DOWN;
		if (ev->value == 1) { push(k, 0, 0); rep_key = k; rep_next = t + 450; }
		else if (ev->value == 0 && rep_key == k) rep_key = 0;
	} else if (fd == touch_fd && ev->type == EV_ABS) {
		if (ev->code == ABS_MT_TRACKING_ID) {
			if (ev->value >= 0 && touch_id < 0) { touch_id = ev->value; touch_t0 = t; tx0 = ty0 = -1; }
			else if (ev->value < 0 && touch_id >= 0) {
				touch_id = -1;
				if (tx0 >= 0 && t - touch_t0 < 800 && abs(tx - tx0) + abs(ty - ty0) < 60)	/* a tap */
					push(E_TAP, ty0 * LW / (tmax_y + 1), LH - 1 - tx0 * LH / (tmax_x + 1));
			}
		} else if (ev->code == ABS_MT_POSITION_X) { tx = ev->value * 800 / (tmax_x + 1); if (tx0 < 0) tx0 = tx; }
		else if (ev->code == ABS_MT_POSITION_Y) { ty = ev->value * 1280 / (tmax_y + 1); if (ty0 < 0) ty0 = ty; }
	}
}

int input_wait(int timeout_ms, struct uiev *e)
{
	long deadline = timeout_ms < 0 ? -1 : now_ms() + timeout_ms;
	for (;;) {
		long t = now_ms();
		if (power_down && !power_long_sent && t - power_down >= LONG_MS) { power_long_sent = 1; push(E_POWER_LONG, 0, 0); }
		if (rep_key && t >= rep_next) { push(rep_key, 0, 0); rep_next = t + REPEAT_MS; }
		if (qhead != qtail) { *e = queue[qhead]; qhead = (qhead + 1) % 16; return 1; }
		if (deadline >= 0 && t >= deadline) return 0;
		long w = deadline < 0 ? -1 : deadline - t;
		if (power_down && !power_long_sent) { long l = power_down + LONG_MS - t; w = w < 0 || l < w ? l : w; }
		if (rep_key) { long r = rep_next - t; w = w < 0 || r < w ? r : w; }
		if (poll(fds, nfds, w < 0 ? -1 : w + 1) <= 0) continue;
		for (int i = 0; i < nfds; i++) {
			struct input_event ev;
			while (read(fds[i].fd, &ev, sizeof ev) == sizeof ev) feed(fds[i].fd, &ev, now_ms());
		}
	}
}

/* ---------- backlight ---------- */
static const int levels[] = {2, 4, 7, 12, 20, 32, 48, 70, 100, 145, 200, 255};
#define NLEVEL (int)(sizeof levels / sizeof *levels)
static int level = 5, night;

void light(int on)
{
	char s[16];
	int v = levels[level];
	if (night) v = v / 3 > 1 ? v / 3 : 1;
	snprintf(s, sizeof s, "%d", on ? v : 0);
	int f = open(BL, O_WRONLY);
	if (f >= 0) { if (write(f, s, strlen(s)) < 0) {} close(f); }
}

void light_night(int n)
{
	if (n != night) { night = n; light(1); }
}

void light_step(int dir)	/* meter: a row of glowing pips in the middle of the screen */
{
	level += dir;
	level = level < 0 ? 0 : level >= NLEVEL ? NLEVEL - 1 : level;
	light(1);
	int w = 760, h = 150, x0 = (LW - w) / 2, y0 = (LH - h) / 2;
	dim(x0 - 20, y0 - 20, x0 + w + 20, y0 + h + 20, 40);
	text_c(F_BAT, LW / 2, y0 - 4, "BRIGHTNESS", rgb(pal_core, .8f));
	for (int i = 0; i < NLEVEL; i++) {
		cellbox c = {x0 + 20 + i * 61, y0 + 60, 50, 70, 0, 0};
		if (i <= level) { c.r = 9; c.g = 14; line(&c, 25, 35, 25, 35, .4f); }
		else fill(c.x + 22, c.y + 32, c.x + 28, c.y + 38, rgb(pal_core, .25f));
	}
	blit(x0 - 20, y0 - 20, x0 + w + 20, y0 + h + 20);
}

/* ---------- CRT on / off: only the rows that change are written, so they cost almost nothing ---------- */
static void sleep_ms(int ms) { usleep(ms * 1000); }

void crt_off(void)	/* ~110 ms: top and bottom close in on a bright line, the line shrinks to a dot and dies */
{
	int cy = LH / 2, h = LH / 2;
	unsigned hot = rgb(pal_core, 1.6f);
	for (int k = 1; k <= 4; k++) {
		int nh = LH / 2 * (4 - k) / 4;
		panel_fill(0, cy - h, LW, cy - nh, 0);
		panel_fill(0, cy + nh, LW, cy + h, 0);
		panel_fill(0, cy - nh - 2, LW, cy - nh, hot);	/* glowing edges riding inwards */
		panel_fill(0, cy + nh, LW, cy + nh + 2, hot);
		h = nh + 2;
		sleep_ms(16);
	}
	panel_fill(0, cy - 3, LW, cy + 3, 0xffffff);
	for (int w = LW / 2; w > 8; w /= 3) {
		sleep_ms(14);
		panel_fill(0, cy - 3, LW / 2 - w, cy + 3, 0);
		panel_fill(LW / 2 + w, cy - 3, LW, cy + 3, 0);
	}
	sleep_ms(30);
	panel_fill(0, 0, LW, LH, 0);
}

void crt_on(void)	/* ~100 ms: a white line flashes across the middle, then the picture opens up and down from it */
{
	int cy = LH / 2;
	panel_fill(0, 0, LW, LH, 0);
	for (int w = 12; w < LW / 2; w *= 3) {
		panel_fill(LW / 2 - w, cy - 2, LW / 2 + w, cy + 2, 0xffffff);
		sleep_ms(12);
	}
	panel_fill(0, cy - 2, LW, cy + 2, 0xffffff);
	unsigned hot = rgb(pal_core, 1.6f);
	int h = 2;
	for (int k = 1; k <= 4; k++) {
		int nh = LH / 2 * k / 4;
		blit(0, cy - nh, LW, cy - h + 2);
		blit(0, cy + h - 2, LW, cy + nh);
		if (k < 4) {
			panel_fill(0, cy - nh, LW, cy - nh + 2, hot);
			panel_fill(0, cy + nh - 2, LW, cy + nh, hot);
		}
		h = nh;
		sleep_ms(16);
	}
	blit(0, 0, LW, LH);
}

/* ---------- quick actions ---------- */
#define TILES 4
#define TW 270
#define TH 320
static const char *labels[TILES] = {"SET TIME", "REBOOT", "POWER OFF", "CLOSE"};

static void arc(const cellbox *c, float cx, float cy, float r, float a0, float a1, float boost)
{
	pt p[64];
	for (int i = 0; i < 64; i++) {
		float a = a0 + (a1 - a0) * i / 63;
		p[i] = (pt){cx + r * cosf(a), cy + r * sinf(a)};
	}
	stroke(c, p, 64, boost);
}

static void icon(int i, const cellbox *c, float boost)
{
	float cx = c->w / 2.f, cy = 120;
	switch (i) {
	case 0: arc(c, cx, cy, 72, 0, 2 * M_PI, boost); line(c, cx, cy, cx, cy - 48, boost); line(c, cx, cy, cx + 36, cy, boost); break;
	case 1: arc(c, cx, cy, 72, -1.2f, 4.2f, boost); line(c, cx + 72 * cosf(-1.2f), cy + 72 * sinf(-1.2f), cx + 50, cy - 90, boost);
		line(c, cx + 72 * cosf(-1.2f), cy + 72 * sinf(-1.2f), cx + 70, cy - 40, boost); break;
	case 2: arc(c, cx, cy, 72, -1.1f + (float)M_PI * 0, 4.25f, boost); line(c, cx, cy - 92, cx, cy - 10, boost); break;
	default: line(c, cx - 55, cy - 55, cx + 55, cy + 55, boost); line(c, cx + 55, cy - 55, cx - 55, cy + 55, boost);
	}
}

static int tile_x(int i) { return (LW - TILES * TW - (TILES - 1) * 26) / 2 + i * (TW + 26); }
#define TY 210

static void draw_menu(int sel)
{
	memcpy(cv, bg, LW * LH * 4);
	text_c(F_BAT, LW / 2, 90, "QUICK ACTIONS", rgb(pal_core, .9f));
	float keep[3];
	memcpy(keep, pal_core, sizeof keep);
	for (int i = 0; i < TILES; i++) {
		float k = i == sel ? 1 : .38f;
		for (int c = 0; c < 3; c++) pal_core[c] = keep[c] * k;
		cellbox c = {tile_x(i), TY, TW, TH, i == sel ? 5 : 3, i == sel ? 16 : 6};
		pt fr[5] = {{14, 14}, {TW - 14, 14}, {TW - 14, TH - 14}, {14, TH - 14}, {14, 14}};
		stroke(&c, fr, 5, i == sel ? .6f : 0);
		c.r = i == sel ? 9 : 7;
		c.g = i == sel ? 18 : 8;
		icon(i, &c, i == sel ? .5f : 0);
		text_c(F_BAT, c.x + TW / 2, TY + TH - 90, labels[i], rgb(pal_core, 1));
	}
	memcpy(pal_core, keep, sizeof keep);
	text_c(F_BAT, LW / 2, 640, "VOL  MOVE    POWER  SELECT    TOUCH  TAP", rgb(pal_core, .35f));
	blit(0, 0, LW, LH);
}

static int set_time(void);

int charging(void)
{
	char st[32] = "";
	int f = open("/sys/class/power_supply/battery/status", O_RDONLY);
	if (f >= 0) { if (read(f, st, sizeof st - 1) < 0) {} close(f); }
	return !strncmp(st, "Charging", 8) || !strncmp(st, "Full", 4);
}

void shutdown(int restart)	/* instant: picture collapses, backlight off, straight into the kernel */
{
	crt_off();
	light(0);
	sync();
	reboot(restart ? RB_AUTOBOOT : RB_POWER_OFF);
	for (;;) pause();	/* charger attached and the PMIC stayed up: nothing left to do */
}

int quick_actions(void)
{
	int sel = 0;
	draw_menu(sel);
	for (;;) {
		struct uiev e;
		if (!input_wait(15000, &e)) return 0;
		int go = -1;
		if (e.type == E_UP) sel = (sel + TILES - 1) % TILES;
		else if (e.type == E_DOWN) sel = (sel + 1) % TILES;
		else if (e.type == E_POWER) go = sel;
		else if (e.type == E_POWER_LONG) return 0;
		else if (e.type == E_TAP)
			for (int i = 0; i < TILES; i++)
				if (e.x >= tile_x(i) && e.x < tile_x(i) + TW && e.y >= TY && e.y < TY + TH) go = sel = i;
		if (go == 0) return set_time();
		if (go == 2 && charging()) { crt_off(); light(0); return 2; }	/* charger holds the PMIC on: sleep */
		if (go == 1 || go == 2) shutdown(go == 1);
		if (go == 3) return 0;
		draw_menu(sel);
	}
}

/* ---------- time & date editor ---------- */
enum { F_HOUR, F_MIN, F_DAY, F_MON, F_YEAR, FIELDS };
static const int lo_v[FIELDS] = {0, 0, 1, 1, 2024}, hi_v[FIELDS] = {23, 59, 31, 12, 2099};
#define TB_W 150
#define TB_H 250
#define TS_W 66
#define TS_H 118

static int mdays(int m, int y) { return m == 2 ? 28 + (y % 4 == 0) : m == 4 || m == 6 || m == 9 || m == 11 ? 30 : 31; }

static void field_box(int f, int *x, int *y, int *w, int *h)
{
	int bx = (LW - 4 * TB_W - 70) / 2, sx = (LW - 8 * TS_W - 2 * 34) / 2;
	static const int sdig[3] = {0, 2, 4};
	if (f <= F_MIN) { *x = bx + f * (2 * TB_W + 70); *y = 130; *w = 2 * TB_W; *h = TB_H; }
	else { *x = sx + sdig[f - F_DAY] * TS_W + (f - F_DAY) * 34; *y = 520; *w = (f == F_YEAR ? 4 : 2) * TS_W; *h = TS_H; }
}

static void draw_set(const int *v, int focus)
{
	memcpy(cv, bg, LW * LH * 4);
	text_c(F_BAT, LW / 2, 26, "SET TIME & DATE", rgb(pal_core, .9f));
	for (int f = 0; f < FIELDS; f++) {
		int x, y, w, h, nd = f == F_YEAR ? 4 : 2, val = v[f];
		field_box(f, &x, &y, &w, &h);
		float keep[3];
		memcpy(keep, pal_core, sizeof keep);
		if (f != focus) for (int c = 0; c < 3; c++) pal_core[c] *= .55f;
		for (int i = nd - 1; i >= 0; i--, val /= 10) {
			cellbox c = {x + i * (w / nd), y, w / nd, h, f <= F_MIN ? 12 : 7, f <= F_MIN ? 16 : 9};
			digit(&c, val % 10, val % 10, 1, 0);
		}
		cellbox ch = {x, y - 70, w, 70, 5, 8};	/* chevrons: up above, down below */
		pt up[3] = {{w / 2.f - 30, 52}, {w / 2.f, 24}, {w / 2.f + 30, 52}};
		stroke(&ch, up, 3, f == focus ? .5f : 0);
		ch.y = y + h;
		pt dn[3] = {{w / 2.f - 30, 18}, {w / 2.f, 46}, {w / 2.f + 30, 18}};
		stroke(&ch, dn, 3, f == focus ? .5f : 0);
		memcpy(pal_core, keep, sizeof keep);
	}
	int x, y, w, h;
	field_box(F_HOUR, &x, &y, &w, &h);
	cellbox colon = {x + w, y, 70, h, 10, 12};
	pt d[1] = {{35, h * .36f}};
	stroke(&colon, d, 1, 0);
	d[0].y = h * .64f;
	stroke(&colon, d, 1, 0);
	for (int f = F_DAY; f < F_YEAR; f++) {	/* dots between date fields */
		field_box(f, &x, &y, &w, &h);
		cellbox dot = {x + w, y, 34, h, 6, 8};
		pt p[1] = {{17, h - 20}};
		stroke(&dot, p, 1, 0);
	}
	text(F_BAT, 120, 712, "CANCEL", rgb(pal_core, .7f));
	text(F_BAT, LW - 120 - text_w(F_BAT, "SAVE"), 712, "SAVE", rgb(pal_core, 1));
	blit(0, 0, LW, LH);
}

static void bump(int *v, int f, int d)
{
	int lo = lo_v[f], hi = f == F_DAY ? mdays(v[F_MON], v[F_YEAR]) : hi_v[f];
	v[f] += d;
	if (v[f] > hi) v[f] = lo;
	if (v[f] < lo) v[f] = hi;
	if (v[F_DAY] > mdays(v[F_MON], v[F_YEAR])) v[F_DAY] = mdays(v[F_MON], v[F_YEAR]);
}

static int save_time(const int *v)
{
	struct tm lt = {0};
	lt.tm_hour = v[F_HOUR]; lt.tm_min = v[F_MIN]; lt.tm_mday = v[F_DAY];
	lt.tm_mon = v[F_MON] - 1; lt.tm_year = v[F_YEAR] - 1900; lt.tm_isdst = -1;
	time_t t = mktime(&lt);
	struct timeval tv = {t, 0};
	settimeofday(&tv, NULL);
	struct tm ut;
	gmtime_r(&t, &ut);
	struct rtc_time r = {ut.tm_sec, ut.tm_min, ut.tm_hour, ut.tm_mday, ut.tm_mon, ut.tm_year, 0, 0, 0};
	int fd = open("/dev/rtc0", O_RDONLY);
	if (fd >= 0) { ioctl(fd, RTC_SET_TIME, &r); close(fd); }
	return 1;
}

static int set_time(void)
{
	time_t now = time(NULL);
	struct tm t;
	localtime_r(&now, &t);
	int v[FIELDS] = {t.tm_hour, t.tm_min, t.tm_mday, t.tm_mon + 1, t.tm_year + 1900}, focus = 0;
	if (v[F_YEAR] < 2024) v[F_YEAR] = 2026;
	draw_set(v, focus);
	for (;;) {
		struct uiev e;
		if (!input_wait(60000, &e)) return 0;
		if (e.type == E_UP) bump(v, focus, 1);
		else if (e.type == E_DOWN) bump(v, focus, -1);
		else if (e.type == E_POWER) { if (++focus == FIELDS) return save_time(v); }
		else if (e.type == E_POWER_LONG) return 0;
		else if (e.type == E_TAP) {
			if (e.y > 690 && e.x < 400) return 0;
			if (e.y > 690 && e.x > LW - 400) return save_time(v);
			for (int f = 0; f < FIELDS; f++) {
				int x, y, w, h;
				field_box(f, &x, &y, &w, &h);
				if (e.x >= x && e.x < x + w && e.y >= y - 70 && e.y < y + h + 70) {
					focus = f;
					bump(v, f, e.y < y + h / 2 ? 1 : -1);
				}
			}
		}
		draw_set(v, focus);
	}
}
