// SPDX-License-Identifier: GPL-2.0
/*
 * MT8163 CPU MTCMOS for secondary cores. The P80h ATF only records the entry point on PSCI CPU_ON; the stock kernel
 * (mt_psci_cpu_boot/kill) then switches the core's power domain itself with spm_mtcmos_ctrl_cpuN. Same sequence here,
 * called from arm64 psci.c after CPU_ON and on CPU kill. Per core n (1..3): PWR_CON 0x214 + 4n, L1_PDN 0x25c + 8n,
 * PWR_STATUS(_2ND) bit 9 + n, standby-WFI 0x720 bit 16 + n (read from the stock binary, checked on live registers).
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/psci.h>
#include <linux/spinlock.h>

#define SPM_POWERON_CONFIG_SET	0x000
#define SPM_PROJECT_UNLOCK	0x0b160001
#define SPM_PWR_STATUS		0x60c
#define SPM_PWR_STATUS_2ND	0x610
#define SPM_SLEEP_TIMER_STA	0x720

#define PWR_RST_B		BIT(0)
#define PWR_CLK_DIS		BIT(1)
#define PWR_ON			BIT(2)
#define PWR_ON_2ND		BIT(3)
#define PWR_ISO			BIT(4)
#define SRAM_CKISO		BIT(5)
#define SRAM_ISOINT_B		BIT(6)
#define L1_PDN			BIT(0)
#define L1_PDN_ACK		BIT(8)

static void __iomem *spm;
static DEFINE_SPINLOCK(spm_lock);

static void spm_rmw(u32 off, u32 clr, u32 set)
{
	writel((readl(spm + off) & ~clr) | set, spm + off);
	dsb(sy);
}

static int spm_wait_status(u32 bit, bool on)
{
	u32 a, b;
	int ret;

	ret = readl_poll_timeout_atomic(spm + SPM_PWR_STATUS, a, !!(a & bit) == on, 1, 10000);
	if (!ret)
		ret = readl_poll_timeout_atomic(spm + SPM_PWR_STATUS_2ND, b, !!(b & bit) == on, 1, 10000);
	return ret;
}

int mt8163_cpu_power(u64 hwid, bool on)
{
	unsigned int n = hwid & 0xff;
	u32 con = 0x214 + 4 * n, l1 = 0x25c + 8 * n, st = BIT(9 + n), v;
	unsigned long flags;
	int ret;

	if (!spm || n < 1 || n > 3 || (hwid & ~0xffULL))
		return -ENODEV;
	writel(SPM_PROJECT_UNLOCK, spm + SPM_POWERON_CONFIG_SET);
	if (!on && readl_poll_timeout_atomic(spm + SPM_SLEEP_TIMER_STA, v, v & BIT(16 + n), 10, 100000))
		pr_warn("mt8163-cpu: CPU%u not in WFI, powering it down anyway\n", n);

	spin_lock_irqsave(&spm_lock, flags);
	if (on) {
		spm_rmw(con, 0, PWR_ON);
		udelay(1);
		spm_rmw(con, 0, PWR_ON_2ND);
		ret = spm_wait_status(st, true);
		spm_rmw(con, PWR_CLK_DIS, 0);
		spm_rmw(l1, L1_PDN, 0);
		if (!ret)
			ret = readl_poll_timeout_atomic(spm + l1, v, !(v & L1_PDN_ACK), 1, 10000);
		udelay(1);
		spm_rmw(con, 0, SRAM_ISOINT_B);
		spm_rmw(con, SRAM_CKISO, 0);
		spm_rmw(con, PWR_ISO, 0);
		spm_rmw(con, 0, PWR_RST_B);
	} else {
		spm_rmw(con, 0, PWR_CLK_DIS);
		spm_rmw(con, 0, SRAM_CKISO);
		spm_rmw(con, SRAM_ISOINT_B, 0);
		spm_rmw(l1, 0, L1_PDN);
		ret = readl_poll_timeout_atomic(spm + l1, v, v & L1_PDN_ACK, 1, 10000);
		spm_rmw(con, PWR_RST_B, 0);
		spm_rmw(con, 0, PWR_ISO);
		spm_rmw(con, PWR_ON, 0);
		spm_rmw(con, PWR_ON_2ND, 0);
		if (!ret)
			ret = spm_wait_status(st, false);
	}
	spin_unlock_irqrestore(&spm_lock, flags);
	if (ret)
		pr_err("mt8163-cpu: CPU%u power %s timed out (con %08x status %08x)\n", n, on ? "on" : "off",
		       readl(spm + con), readl(spm + SPM_PWR_STATUS));
	return ret;
}

int psci_platform_cpu_power(u64 hwid, bool on)
{
	return mt8163_cpu_power(hwid, on);
}

/* SMP bring-up runs right after the early initcalls, before any driver probes */
static int __init mt8163_cpu_power_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-scpsys");

	if (!np)
		return 0;
	spm = of_iomap(np, 0);
	of_node_put(np);
	return spm ? 0 : -ENOMEM;
}
early_initcall(mt8163_cpu_power_init);
