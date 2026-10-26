/*
 * p80h-clock: the tablet as a phosphor desk clock, run from the initramfs instead of the rootfs. Landscape, HH:MM
 * as glowing vector digits that morph when they change, breathing colon dots, seconds, date, battery and a day bar
 * (00-24 h). Colours drift with the time of day; nights morph slower and dim the backlight. Only changed regions
 * are redrawn; between animations it sleeps in poll(). One CPU core.
 * Keys: volume = brightness, power click or screen tap = display off/on (CRT animation), long power = quick actions (set time &
 * reboot, power off). Time from the MT6323 RTC (kernel RTC_HCTOSYS, UTC); zone /etc/p80h/tz, default CET/CEST.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "face.h"
#include "ui.h"

#define BAT "/sys/class/power_supply/battery/"

static cellbox big[4], colon, sec[2];
static int shown[6], from[6];
static long start[6];

static int rd(const char *path, char *out, int n)
{
	int f = open(path, O_RDONLY), k = f < 0 ? 0 : read(f, out, n - 1);
	if (f >= 0) close(f);
	if (k < 0) k = 0;
	out[k] = 0;
	if (k && out[k - 1] == '\n') out[k - 1] = 0;
	return k;
}

static void layout(void) { face_layout(big, &colon, sec); }

static void draw_colon(float k)	/* k: 0 off .. 1 on */
{
	restore(colon.x, colon.y, colon.x + colon.w, colon.y + colon.h);
	if (k > 0.02f) {
		float save[3];
		memcpy(save, pal_core, sizeof save);
		for (int c = 0; c < 3; c++) pal_core[c] = pal_bg[c] + (pal_core[c] - pal_bg[c]) * k;
		pt d[2] = {{COLON_W / 2.f, BIG_H * .35f}, {COLON_W / 2.f, BIG_H * .65f}};
		stroke(&colon, &d[0], 1, k - 1);
		stroke(&colon, &d[1], 1, k - 1);
		memcpy(pal_core, save, sizeof save);
	}
	blit(colon.x, colon.y, colon.x + colon.w, colon.y + colon.h);
}

static void draw_bar(const struct tm *t)
{
	int x0 = 30, x1 = LW - 30, y = BAR;
	float f = (t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec) / 86400.f;
	restore(0, y - 34, LW, LH);
	unsigned dim_c = rgb(pal_core, .2f), on = rgb(pal_core, .9f);
	for (int h = 0; h <= 24; h++) {	/* hour ticks, taller every 6 h */
		int x = x0 + (x1 - x0) * h / 24, tall = h % 6 ? 8 : 18;
		fill(x - 1, y - 6 - tall, x + 2, y - 6, h <= f * 24 ? on : dim_c);
	}
	int xf = x0 + (x1 - x0) * f;
	for (int yy = y; yy < y + 10; yy++)
		for (int xx = x0; xx < x1; xx++) cv[yy * LW + xx] = xx <= xf ? mix(dim_c, on, 60 + 195 * (xx - x0) / (xf - x0 + 1)) : dim_c;
	cellbox head = {xf - 28, y - 23, 56, 56, 7, 20};	/* glowing head */
	line(&head, 28, 28, 28, 28, .8f);
	blit(0, y - 34, LW, LH);
}

static void draw_info(const struct tm *t)
{
	char date[48], cap[16], st[32], msg[48];
	int x0 = sec[1].x + sec[1].w + 10;
	restore(x0, ROW, LW, BAR - 34);
	strftime(date, sizeof date, "%A %d %b %Y", t);
	for (char *c = date; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
	text(F_DATE, LW - 30 - text_w(F_DATE, date), ROW + 14, date, rgb(pal_core, .8f));
	if (rd(BAT "capacity", cap, sizeof cap)) {
		rd(BAT "status", st, sizeof st);
		snprintf(msg, sizeof msg, "BATTERY %s%%%s", cap, !strcmp(st, "Charging") ? "  CHARGING" : !strcmp(st, "Full") ? "  FULL" : "");
		text(F_BAT, LW - 30 - text_w(F_BAT, msg), ROW + 90, msg, rgb(pal_core, .95f));
	}
	blit(x0, ROW, LW, BAR - 34);
}

/* the whole face; digits morph in from `seed` (-1: drawn straight) */
static void face(const struct tm *t, int seed)
{
	palette(t->tm_hour + t->tm_min / 60.f);
	background();
	memcpy(cv, bg, LW * LH * 4);
	blit(0, 0, LW, LH);
	for (int i = 0; i < 6; i++) { shown[i] = seed; start[i] = 0; }
	draw_bar(t);
	draw_info(t);
}

/* one frame: start morphs for changed digits, advance running ones; returns 1 while anything animates */
static int tick(long ms, const struct tm *t, int dur)
{
	int busy = 0, now_d[6] = {t->tm_hour / 10, t->tm_hour % 10, t->tm_min / 10, t->tm_min % 10, t->tm_sec / 10, t->tm_sec % 10};
	for (int i = 0; i < 6; i++) {
		const cellbox *c = i < 4 ? &big[i] : &sec[i - 4];
		if (now_d[i] != shown[i] && !start[i]) {
			from[i] = shown[i] < 0 ? now_d[i] : shown[i];
			start[i] = shown[i] < 0 ? ms - 100000 : ms;
			shown[i] = now_d[i];
		}
		if (start[i]) {
			int len = i < 4 ? dur : 300;
			float k = (ms - start[i]) / (float)len;
			if (k >= 1) { k = 1; start[i] = 0; } else busy = 1;
			restore(c->x, c->y, c->x + c->w, c->y + c->h);
			digit(c, from[i], shown[i], k, i < 4 ? .9f : .5f);
			blit(c->x, c->y, c->x + c->w, c->y + c->h);
		}
	}
	int ph = ms % 1000;	/* colon: on for half a second, fades out, fades back in before the next */
	float k = ph < 500 ? 1 : ph < 750 ? 1 - (ph - 500) / 250.f : ph < 900 ? 0 : (ph - 900) / 100.f;
	draw_colon(k);
	return busy || (ph >= 500 && ph < 750) || ph >= 900;
}

static long wall_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000;
}

static struct tm local(long ms)
{
	time_t s = ms / 1000;
	struct tm t;
	localtime_r(&s, &t);
	return t;
}

int main(void)
{
	char tz[64], path[64];
	setenv("TZ", rd("/etc/p80h/tz", tz, sizeof tz) ? tz : "CET-1CEST,M3.5.0,M10.5.0/3", 1);
	tzset();
	if (gfx_open()) return 1;
	for (int i = 1; i < 4; i++) {	/* one core is plenty */
		snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/online", i);
		int f = open(path, O_WRONLY);
		if (f >= 0) { if (write(f, "0", 1) < 0) {} close(f); }
	}
	input_open();
	layout();
	struct tm t = local(wall_ms());
	palette(t.tm_hour + t.tm_min / 60.f);
	background();
	face(&t, 10);	/* the boot frame (same as the bootloader logo): dashes, which morph into the time */
	light(1);

	int on = 1, tint = t.tm_hour * 6 + t.tm_min / 10, last_min = t.tm_min;
	long overlay_until = 0;
	for (;;) {
		long ms = wall_ms();
		t = local(ms);
		int night = t.tm_hour >= 22 || t.tm_hour < 6, busy = 0;
		if (on) {
			light_night(night);
			if (overlay_until && ms > overlay_until) { overlay_until = 0; face(&t, -1); }
			if (tint != t.tm_hour * 6 + t.tm_min / 10) { tint = t.tm_hour * 6 + t.tm_min / 10; face(&t, -1); }
			if (!overlay_until) {
				busy = tick(ms, &t, night ? 650 : 420);
				if (t.tm_min != last_min) {
					char cap[8];
					if (rd(BAT "capacity", cap, sizeof cap) && atoi(cap) <= 2 && !charging()) shutdown(0);	/* save the cell */
					draw_bar(&t); draw_info(&t); last_min = t.tm_min;
				}
			}
		}
		ms = wall_ms();
		int ph = ms % 1000, wait = -1;
		if (on) wait = busy ? 33 : ph < 500 ? 500 - ph : ph < 900 ? 900 - ph : 1000 - ph + 2;
		if (overlay_until) wait = 100;
		struct uiev e;
		if (!input_wait(wait, &e)) continue;
		if (e.type == E_POWER || e.type == E_TAP) {	/* one click (power key or a tap): display off / on */
			if (on) { crt_off(); light(0); on = 0; }
			else { t = local(wall_ms()); face(&t, -1); tick(wall_ms(), &t, 1); light(1); crt_on(); on = 1; }
		} else if (e.type == E_POWER_LONG) {
			if (!on) { light(1); on = 1; }
			if (quick_actions() == 2) { on = 0; continue; }
			t = local(wall_ms());
			face(&t, -1);
			tint = t.tm_hour * 6 + t.tm_min / 10;
			last_min = t.tm_min;
		} else if (on && (e.type == E_UP || e.type == E_DOWN)) {
			light_step(e.type == E_UP ? 1 : -1);
			overlay_until = wall_ms() + 1500;
		}
	}
}
