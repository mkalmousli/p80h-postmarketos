/* p80h-clock interaction: keys + touch, backlight, CRT on/off, quick actions, time & date editor */
#ifndef P80H_UI_H
#define P80H_UI_H

enum { E_NONE, E_POWER, E_POWER_LONG, E_UP, E_DOWN, E_TAP };
struct uiev { int type, x, y; };	/* x, y: logical canvas pixels for E_TAP */

void input_open(void);
int input_wait(int timeout_ms, struct uiev *e);	/* 1: event, 0: timeout */
long now_ms(void);

void light(int on);	/* backlight at the chosen level (dimmed at night) or off */
void light_step(int dir);	/* volume keys: one level up / down, with an on-screen meter */
void light_night(int night);

void crt_off(void);	/* collapse to a line, then a dot */
void crt_on(void);	/* the reverse, revealing the canvas */
int quick_actions(void);	/* long power press menu; returns 1 time changed, 2 go to sleep */
int charging(void);
void shutdown(int restart);

#endif
