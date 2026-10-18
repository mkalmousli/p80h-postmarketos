// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 CPUXGPT: the block that clocks the ARM generic timer counter (13 MHz from the
 * 26 MHz crystal). Registers are reached through an index/data pair; register layout from the
 * vendor cpuxgpt_mt8163.c (Copyright MediaTek Inc., GPL-2.0).
 */
#include <linux/bits.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <asm/sysreg.h>

#define CTL_DATA	0x0	/* mcucfg + 0x670 */
#define CTL_INDEX	0x4	/* mcucfg + 0x674 */
#define INDEX_CTL_REG	0x0
#define EN_CPUXGPT	BIT(0)
#define CLK_DIV_MASK	GENMASK(10, 8)
#define CLK_DIV2	(2 << 8)

static int __init mt8163_cpuxgpt_init(struct device_node *np)
{
	void __iomem *base = of_iomap(np, 0);
	u64 before = read_sysreg(cntvct_el0);
	u32 ctl;

	if (!base)
		return -ENOMEM;
	writel(INDEX_CTL_REG, base + CTL_INDEX);
	ctl = readl(base + CTL_DATA);
	if (!(ctl & CLK_DIV_MASK))
		ctl |= CLK_DIV2;
	writel(ctl | EN_CPUXGPT, base + CTL_DATA);
	pr_info("cpuxgpt: ctl %#x -> %#x, cntvct %llu -> %llu\n", ctl, readl(base + CTL_DATA), before,
		read_sysreg(cntvct_el0));
	return 0;
}
TIMER_OF_DECLARE(mt8163_cpuxgpt, "mediatek,mt8163-cpuxgpt", mt8163_cpuxgpt_init);
