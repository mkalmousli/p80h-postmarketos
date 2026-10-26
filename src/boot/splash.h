/* p80h-splash shared bits: the landscape canvas turned onto the portrait panel, accent colour, progress bar */
#ifndef SPLASH_H
#define SPLASH_H
#define LW 1280
#define LH 800
extern const float acc[3];	/* P80h accent, #4cc3ff */

static inline void px(unsigned char *fb, int stride, int x, int y, unsigned c)
{
	if ((unsigned)x < LW && (unsigned)y < LH) ((unsigned *)(fb + x * stride))[LH - 1 - y] = c;
}

int ui_open(void);	/* bind the abstract datagram socket "p80h-ui"; 0 on success */
int ui_poll(char *label, int n, int *pm);	/* next "<label>\t<permille>" from p80hd; 1 if there was one, pm -1 = unknown */
void ui_text(unsigned char *fb, int stride, int x, int y, const char *s, unsigned fg, unsigned bg, int align);	/* -1 left, 0 centre, 1 right of x */
void bar_set(const char *label, int pm, float t);	/* an action update; empty label = done */
int bar_draw(unsigned char *fb, int stride, float t);	/* once per frame; 1 while the bar is visible or sliding */
#endif
