// SPDX-License-Identifier: GPL-2.0
/*
 * Bring-up console: mirrors the kernel log into the spare (never displayed) page of the video
 * memory LK reserves at the top of DRAM. After a watchdog reset the stock 3.10 kernel exposes
 * that memory as /dev/fb0, so `p80h log` can read the mainline log back without any storage driver.
 *
 * Layout at LOG_BASE: "P80HLOG\0", u32 length, then text.
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>

#define LOG_BASE	0x7f000000UL	/* 4 MB into the 12 MB stock framebuffer: its third page */
#define LOG_SIZE	0x100000UL
#define HDR		16

static u8 __iomem *log;
static u32 len;

static void ramlog_write(struct console *con, const char *s, unsigned int n)
{
	if (n > LOG_SIZE - HDR - len)
		n = LOG_SIZE - HDR - len;
	memcpy_toio(log + HDR + len, s, n);
	len += n;
	writel(len, log + 8);
}

static struct console ramlog_console = {
	.name = "ramlog",
	.write = ramlog_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED,
	.index = -1,
};

static int __init ramlog_init(void)
{
	log = ioremap(LOG_BASE, LOG_SIZE);
	if (!log)
		return 0;
	memcpy_toio(log, "P80HLOG", 8);
	writel(0, log + 8);
	register_console(&ramlog_console);
	return 0;
}
console_initcall(ramlog_init);
