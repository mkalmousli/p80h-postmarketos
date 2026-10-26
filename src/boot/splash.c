/* p80h-splash: the quiet boot screen. A comet circles a ring over black, a core dot breathes; SIGTERM fades it out.
 * Draws straight to /dev/fb0: logical landscape 1280x800 turned 90 degrees clockwise onto the portrait panel. */
#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "splash.h"
#define R 74.f		/* ring radius, px */
#define BOX 100		/* half size of the redrawn square */
static volatile sig_atomic_t quit;

static float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3f + t.tv_nsec / 1e6f; }

#define SZ (2 * BOX)
#define TURN 1024
static float base[SZ][SZ], d_[SZ][SZ], tail_lut[TURN];	/* static per pixel: ring + halo, distance; tail by turn fraction */
static short ang[SZ][SZ];	/* angle in 1/TURN of a turn */

static void tables(void)
{
	for (int y = 0; y < SZ; y++)
		for (int x = 0; x < SZ; x++) {
			float dx = x - BOX, dy = y - BOX, d = sqrtf(dx * dx + dy * dy);
			d_[y][x] = d;
			base[y][x] = sat(2.5f - fabsf(d - R)) * .14f + sat(1 - d / 40) * .06f;	/* 5 px ring line, centre halo */
			ang[y][x] = (short)((atan2f(dy, dx) + 3.14159f) / 6.2832f * TURN) % TURN;
		}
	for (int i = 0; i < TURN; i++) tail_lut[i] = powf(1 - (float)i / TURN, 3);	/* 1 at the head, 0 a turn behind */
}

static void frame(unsigned char *fb, int stride, float k, float t)
{
	float breath = .5f + .5f * sinf(t * .0035f);
	int head = (int)(t * .0042f / 6.2832f * TURN) % TURN;
	for (int y = 0; y < SZ; y++)
		for (int x = 0; x < SZ; x++) {
			float d = d_[y][x], ring = sat(2.5f - fabsf(d - R));
			float l = base[y][x] + ring * tail_lut[(head - ang[y][x] + TURN) % TURN];	/* the comet */
			if (d < 10) l += sat(6 + 4 * breath - d) * (.5f + .5f * breath);
			l = sat(l) * k;
			unsigned px = (unsigned)(acc[0] * l) << 16 | (unsigned)(acc[1] * l) << 8 | (unsigned)(acc[2] * l);
			((unsigned *)(fb + (LW / 2 - BOX + x) * stride))[LH - 1 - (LH / 2 - BOX + y)] = px;
		}
}

static void stop(int s) { (void)s; quit = 1; }

int main(int argc, char **argv)
{
	int over = argc > 1;	/* --bar: the desktop owns the screen, draw only the progress bar over it */
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo fx;
	int fd = -1;
	for (int i = 0; i < 100 && (fd = open("/dev/fb0", O_RDWR)) < 0; i++) usleep(100000);
	if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &fx) ||
	    v.bits_per_pixel != 32 || v.xres != LH || v.yres != LW)
		return 1;
	unsigned char *fb = mmap(NULL, fx.line_length * v.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED) return 1;
	if (!over) {
		for (unsigned i = 0; i < fx.line_length * v.yres / 4; i++) ((unsigned *)fb)[i] = 0;
		v.yoffset = 0;
		ioctl(fd, FBIOPAN_DISPLAY, &v);	/* make the display (re)scan fb0, e.g. after X left */
	}
	tables();
	ui_open();
	signal(SIGTERM, stop);
	signal(SIGINT, stop);
	float t0 = ms(), tq = 0, seen = 0;
	char label[96] = "";
	int pm;
	for (;;) {
		float t = ms() - t0, k = sat(t / 600);	/* fade in */
		if (ui_poll(label, sizeof(label), &pm)) bar_set(label, pm, seen = t);	/* an action from p80hd */
		if (!over && t - seen > 90000) quit = 1;	/* never outlive a desktop that forgot to stop us */
		if (quit && !tq) tq = t, bar_set("", 0, t);
		if (quit) k = sat(1 - (t - tq) / 200) * k;
		if (!over) frame(fb, fx.line_length, k, t);
		int busy = bar_draw(fb, fx.line_length, t);
		if (quit && k <= 0) return 0;
		usleep(over && !busy ? 100000 : 16000);
	}
}
