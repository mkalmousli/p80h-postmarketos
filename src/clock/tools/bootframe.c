/*
 * bootframe (runs on the PC): renders the bootloader's full-screen pictures with the clock's own renderer, so
 * LK's screens match the clock (landscape, phosphor glow), written as the 800x1280 portrait panel in BGRA on stdout.
 *   bootframe boot           the boot logo: the clock face with dashes, which the clock then morphs into the time
 *   bootframe charge PCT     power-off charging, battery filled to PCT
 *   bootframe low | error    battery too low to start / charger error
 * Built by src/p80h/re/logo.py together with ../gfx.c (host gcc).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../face.h"

static void dots(const cellbox *colon)
{
	pt d[2] = {{COLON_W / 2.f, BIG_H * .35f}, {COLON_W / 2.f, BIG_H * .65f}};
	stroke(colon, &d[0], 1, 0);
	stroke(colon, &d[1], 1, 0);
}

static void day_bar(void)	/* empty: ticks only, as the clock draws it before it knows the time */
{
	int x0 = 30, x1 = LW - 30;
	for (int h = 0; h <= 24; h++) {
		int x = x0 + (x1 - x0) * h / 24, tall = h % 6 ? 8 : 18;
		fill(x - 1, BAR - 6 - tall, x + 2, BAR - 6, rgb(pal_core, .2f));
	}
	fill(x0, BAR, x1, BAR + 10, rgb(pal_core, .2f));
}

static void battery(int pct, float k)	/* a big glowing cell in the middle, filled to pct */
{
	int w = 760, h = 320, x0 = (LW - w) / 2 - 20, y0 = 150;
	cellbox c = {x0, y0, w + 80, h, 9, 18};
	float keep[3];
	memcpy(keep, pal_core, sizeof keep);
	for (int i = 0; i < 3; i++) pal_core[i] *= k;
	pt body[5] = {{30, 30}, {w - 10, 30}, {w - 10, h - 30}, {30, h - 30}, {30, 30}};
	stroke(&c, body, 5, .3f);
	line(&c, w + 30, h / 2.f - 50, w + 30, h / 2.f + 50, .3f);	/* the terminal */
	int fw = (w - 100) * pct / 100;
	for (int x = 0; x + 30 < fw; x += 50) {	/* level as glowing bars */
		cellbox b = {x0 + 60 + x, y0 + 70, 40, h - 140, 12, 10};
		line(&b, 20, 22, 20, h - 162, .4f);
	}
	memcpy(pal_core, keep, sizeof keep);
}

int main(int argc, char **argv)
{
	static unsigned char panel[LW * LH * 4];
	cellbox big[4], colon, sec[2];
	if (argc < 2 || gfx_mem(panel, LH * 4)) return 1;
	palette(12);	/* daytime green: the bootloader does not know the time */
	background();
	memcpy(cv, bg, LW * LH * 4);
	face_layout(big, &colon, sec);
	if (!strcmp(argv[1], "boot")) {
		for (int i = 0; i < 4; i++) digit(&big[i], 10, 10, 1, 0);
		for (int i = 0; i < 2; i++) digit(&sec[i], 10, 10, 1, 0);
		dots(&colon);
		text(F_DATE, LW - 30 - text_w(F_DATE, "STARTING"), ROW + 14, "STARTING", rgb(pal_core, .8f));
		day_bar();
	} else if (!strcmp(argv[1], "charge")) {
		int pct = argc > 2 ? atoi(argv[2]) : 50;
		battery(pct, 1);
		text_c(F_DATE, LW / 2, 540, pct >= 100 ? "CHARGED" : "CHARGING", rgb(pal_core, .9f));
		text_c(F_BAT, LW / 2, 620, "POWER KEY TO START", rgb(pal_core, .45f));
	} else {
		int low = !strcmp(argv[1], "low");
		battery(low ? 4 : 0, low ? .8f : .5f);
		text_c(F_DATE, LW / 2, 540, low ? "BATTERY LOW" : "CHARGER ERROR", rgb(pal_core, .9f));
		text_c(F_BAT, LW / 2, 620, low ? "CONNECT THE CHARGER" : "UNPLUG THE CHARGER", rgb(pal_core, .45f));
	}
	blit(0, 0, LW, LH);
	fwrite(panel, 1, sizeof panel, stdout);
	return 0;
}
