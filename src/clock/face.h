/* clock face geometry, shared by the clock and the bootloader artwork (tools/bootframe.c) */
#ifndef P80H_FACE_H
#define P80H_FACE_H
#include "gfx.h"

#define BIG_W 282
#define BIG_H 540
#define COLON_W 102
#define ROW 560	/* seconds / date / battery row */
#define BAR 762	/* day bar */

static inline void face_layout(cellbox *big, cellbox *colon, cellbox *sec)
{
	int x = (LW - 4 * BIG_W - COLON_W) / 2;
	for (int i = 0; i < 4; i++) {
		big[i] = (cellbox){x, 0, BIG_W, BIG_H, 22, 30};
		x += BIG_W + (i == 1 ? COLON_W : 0);
	}
	*colon = (cellbox){big[1].x + BIG_W, 0, COLON_W, BIG_H, 18, 30};
	for (int i = 0; i < 2; i++) sec[i] = (cellbox){12 + i * 100, ROW - 6, 100, 178, 9, 14};
}
#endif
