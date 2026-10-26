/* the bottom progress bar: slides up on the first message, label left, "42%  ETA 0:32" right, an eased fill along
 * the top edge (a sweeping highlight while the size is unknown), slides down when p80hd says it is done */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "splash.h"

#define BH 72	/* bar height */
#define TRK 6	/* progress track height */
#define BG 0x0d1620u
#define FG 0xe8f4ffu
#define ACC ((unsigned)acc[0] << 16 | (unsigned)acc[1] << 8 | (unsigned)acc[2])
static char label[96];
static float target = -1, shown, t0, tlast, appear, eta = -1;
static int active;

static unsigned shade(unsigned c, float k) { return (unsigned)((c >> 16 & 255) * k) << 16 | (unsigned)((c >> 8 & 255) * k) << 8 | (unsigned)((c & 255) * k); }

void bar_set(const char *l, int pm, float t)
{
	if (!*l) {
		active = 0;
		return;
	}
	if (!active || strcmp(l, label)) {	/* a new action: restart the fill and the ETA */
		snprintf(label, sizeof(label), "%s", l);
		t0 = t, shown = 0, eta = -1;
	}
	active = 1;
	target = pm < 0 ? -1 : pm / 1000.f;
}

static void fill(unsigned char *fb, int stride, int x0, int y0, int x1, int y1, unsigned c)
{
	for (int x = x0; x < x1; x++)
		for (int y = y0; y < y1; y++) px(fb, stride, x, y, c);
}

int bar_draw(unsigned char *fb, int stride, float t)
{
	float dt = t - tlast, e, y;
	char r[40];
	tlast = t;
	appear += (active ? dt : -dt) / 260;
	appear = appear < 0 ? 0 : appear > 1 ? 1 : appear;
	if (appear <= 0 && !active) return 0;
	e = 1 - powf(1 - appear, 3);	/* ease-out slide */
	y = LH - BH * e;
	fill(fb, stride, 0, LH - BH, LW, (int)y, 0);	/* what the bar uncovered while sliding down */
	if (target >= 0) shown += (target - shown) * (1 - expf(-dt / 140));	/* smooth fill */
	if (target > .03f && t - t0 > 1500) {	/* remaining = elapsed * (1 - done) / done, low-passed */
		float now = (t - t0) / 1000 * (1 - target) / target;
		eta = eta < 0 ? now : eta * .96f + now * .04f;
	}
	fill(fb, stride, 0, (int)y, LW, LH, BG);
	fill(fb, stride, 0, (int)y, LW, (int)y + TRK, shade(ACC, .18f));
	if (target >= 0)
		fill(fb, stride, 0, (int)y, (int)(shown * LW), (int)y + TRK, ACC);
	else {	/* unknown size: a highlight sweeps along the track */
		int x = (int)(fmodf(t / 1100, 1.25f) * LW) - LW / 4;
		for (int i = 0; i < LW / 4; i++) {
			float k = sinf(3.1416f * i / (LW / 4));
			for (int j = 0; j < TRK; j++) px(fb, stride, x + i, (int)y + j, shade(ACC, .18f + .82f * k));
		}
	}
	ui_text(fb, stride, 32, (int)y + TRK + 18, label, FG, BG, -1);
	if (target >= 0) {
		int s = (int)(eta + .5f), n = snprintf(r, sizeof(r), "%d%%", (int)(shown * 100 + .5f));
		if (eta >= 0 && s < 6000) snprintf(r + n, sizeof(r) - n, "   ETA %d:%02d", s / 60, s % 60);
		ui_text(fb, stride, LW - 32, (int)y + TRK + 18, r, FG, BG, 1);
	}
	return 1;
}
