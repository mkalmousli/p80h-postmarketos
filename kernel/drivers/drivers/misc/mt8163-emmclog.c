// SPDX-License-Identifier: GPL-2.0
/* Bring-up console: printk into raw eMMC sectors via polled MSDC0 PIO (bootloader left it set up); read back
 * with dd. Registers as in mainline drivers/mmc/host/mtk-sd.c. */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>

#define BASE	0x11230000UL
#define FIRST	258560		/* sector of the "expdb" partition (crash dumps, safe to overwrite) */
#define BLOCKS	512
#define MSDC_CFG	0x00
#define MSDC_PS		0x08
#define MSDC_INT	0x0c
#define MSDC_FIFOCS	0x14
#define MSDC_TXDATA	0x18
#define SDC_CFG		0x30
#define SDC_CMD		0x34
#define SDC_ARG		0x38
#define SDC_BLK_NUM	0x50
#define INT_CMDRDY	BIT(8)
#define INT_ERR		(BIT(9) | BIT(10) | BIT(14) | BIT(15))
#define INT_XFER	BIT(12)
#define WAIT		2000000

static void __iomem *m;
static char buf[BLOCKS * 512];
static unsigned int len, done, fails;

static int wait_int(u32 bit)
{
	u32 v;
	int i;
	for (i = 0; i < WAIT; i++) {
		v = readl(m + MSDC_INT);
		if (v & (bit | INT_ERR)) {
			writel(v, m + MSDC_INT);
			return v & INT_ERR ? -EIO : 0;
		}
	}
	return -ETIMEDOUT;
}
static int put_block(unsigned int n)
{
	const u32 *w = (const u32 *)(buf + n * 512);
	int i, j, r;
	writel(readl(m + MSDC_CFG) | BIT(3), m + MSDC_CFG);	/* PIO mode */
	writel(readl(m + MSDC_INT), m + MSDC_INT);
	writel(BIT(31), m + MSDC_FIFOCS);			/* clear fifo */
	writel(1, m + SDC_BLK_NUM);
	writel(FIRST + n, m + SDC_ARG);
	writel(24 | 1 << 7 | 1 << 11 | 1 << 13 | 512 << 16, m + SDC_CMD);	/* CMD24, R1, single block write */
	if ((r = wait_int(INT_CMDRDY)))
		return r;
	for (i = 0; i < 4; i++) {	/* 128-byte fifo: refill when it has drained */
		for (j = 0; j < WAIT && (readl(m + MSDC_FIFOCS) >> 16 & 0xff); j++)
			;
		for (j = 0; j < 32; j++)
			writel(*w++, m + MSDC_TXDATA);
	}
	if ((r = wait_int(INT_XFER)))
		return r;
	for (i = 0; i < WAIT && !(readl(m + MSDC_PS) & BIT(16)); i++)	/* DAT0 low = card programming */
		;
	return 0;
}

static void flush(void)
{
	for (; done <= len / 512 && done < BLOCKS && fails < 3; done++)
		if (put_block(done))
			fails++;
	done = len / 512;	/* the partial block is rewritten on the next call */
}

static void emmclog_write(struct console *con, const char *s, unsigned int n)
{
	unsigned int room = sizeof(buf) - len;

	memcpy(buf + len, s, min(n, room));
	len += min(n, room);
	flush();
}
static struct console emmclog_console = {
	.name = "emmclog",
	.write = emmclog_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED,
	.index = -1,
};
static int __init emmclog_init(void)
{
	m = ioremap(BASE, 0x100);
	if (!m)
		return 0;
	len = scnprintf(buf, 200, "emmclog cfg=%08x ps=%08x sdc=%08x\n", readl(m + MSDC_CFG), readl(m + MSDC_PS),
			readl(m + SDC_CFG));
	flush();
	register_console(&emmclog_console);
	return 0;
}
console_initcall(emmclog_init);
