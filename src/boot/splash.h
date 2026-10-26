/* p80h-splash shared bits: the landscape canvas turned onto the portrait panel, accent colour, progress messages */
#ifndef SPLASH_H
#define SPLASH_H
#define LW 1280
#define LH 800
extern const float acc[3];	/* P80h accent, #4cc3ff */
int ui_open(void);	/* bind the abstract datagram socket "p80h-ui"; 0 on success */
int ui_poll(char *label, int n, int *pct);	/* next "<label>\t<pct>" from p80hd; 1 if there was one, pct -1 = unknown */
void ui_text(unsigned char *fb, int stride, int cx, int y, const char *s, float k);	/* centred label, k 0..1 */
void ui_clear(unsigned char *fb, int stride, int cx, int y);	/* blank the label strip */
#endif
