// SPDX-License-Identifier: GPL-2.0
/*
 * Bring-up console: mirrors the kernel log into the DRAM block that LK and the stock 3.10 kernel use
 * as their persistent RAM console (it survives the watchdog reset). The stock kernel keeps a copy and
 * `cat /proc/last_kmsg` returns it, so `p80h try-kernel` reads the mainline log without any storage.
 *
 * Header fields use the layout of the vendor struct ram_console_buffer (mtk_ram_console.c, MediaTek
 * Inc., GPL-2.0); the text is a ring starting at TEXT. Offsets with no entry stay zero.
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>

#define LOG_BASE	0x44400000UL	/* CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR of the stock kernel */
#define LOG_SIZE	0x10000UL	/* CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE: stock rejects other sizes */
#define TEXT		512
#define SIG		0x43474244
#define SZ_BUFFER	40
#define LOG_START	52		/* ring write position */
#define LOG_SIZE_FLD	56		/* valid bytes */

static u8 __iomem *log;
static u32 pos, valid;
static bool cached;	/* fell back to the linear map: caches must be cleaned before a reset */

static void ramlog_write(struct console *con, const char *s, unsigned int n)
{
	while (n--) {
		writeb(*s++, log + TEXT + pos);
		pos = (pos + 1) % (LOG_SIZE - TEXT);
		if (valid < LOG_SIZE - TEXT)
			valid++;
	}
	writel(pos, log + LOG_START);
	writel(valid, log + LOG_SIZE_FLD);
	if (cached)
		dcache_clean_poc((unsigned long)log, (unsigned long)log + LOG_SIZE);
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
	if (!log) {	/* LK may drop the reserved-memory node: ioremap() of mapped RAM fails */
		log = memremap(LOG_BASE, LOG_SIZE, MEMREMAP_WB);
		cached = true;
	}
	if (!log)
		return 0;
	memset_io(log, 'M', LOG_SIZE);	/* bring-up probe: a wholly 'M' buffer in last_kmsg proves the region is shared */
	memset_io(log, 0, TEXT);
	writel(SIG, log);
	writel(LOG_SIZE, log + SZ_BUFFER);
	register_console(&ramlog_console);
	return 0;
}
console_initcall(ramlog_init);
