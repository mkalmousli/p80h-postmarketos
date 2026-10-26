/* p80h-clock drawing: logical landscape canvas (LW x LH) over the portrait panel, glowing vector strokes, text */
#ifndef P80H_GFX_H
#define P80H_GFX_H

#define LW 1280
#define LH 800
#define N 96	/* points per digit path */

typedef struct { float x, y; } pt;
typedef struct { int x, y, w, h; float r, g; } cellbox;	/* a stroke of radius r with a glow g, clipped to the box */
enum font { F_DATE, F_BAT };	/* JetBrains Mono 50 / 40 bold (font.h, only in gfx.c) */

extern unsigned *bg, *cv;	/* background and canvas, logical */
extern float pal_core[3], pal_glow[3], pal_bg[3];

int gfx_open(void);	/* framebuffer, buffers, digit shapes; 0 on success */
int gfx_mem(unsigned char *panel, int panel_stride);	/* the same, drawing into memory */
void palette(float hour);
void background(void);
unsigned rgb(const float *c, float k);
unsigned mix(unsigned a, unsigned b, int t);
unsigned add(unsigned a, unsigned b, int t);
void restore(int x0, int y0, int x1, int y1);	/* canvas <- background */
void blit(int x0, int y0, int x1, int y1);	/* canvas -> panel */
void panel_fill(int x0, int y0, int x1, int y1, unsigned col);	/* straight to the panel, canvas untouched */
void dim(int x0, int y0, int x1, int y1, int keep);	/* canvas *= keep / 255 */
void fill(int x0, int y0, int x1, int y1, unsigned col);
void stroke(const cellbox *c, const pt *p, int n, float boost);	/* n points in box pixels; n == 1 is a dot */
void line(const cellbox *c, float x0, float y0, float x1, float y1, float boost);
void digit(const cellbox *c, int a, int b, float t, float boost);	/* digit a morphing into b, t 0..1 (lean glow until 1); 10 = dash */
void text(enum font f, int x, int y, const char *s, unsigned col);
int text_w(enum font f, const char *s);
void text_c(enum font f, int cx, int y, const char *s, unsigned col);	/* centred */

#endif
