// SPDX-License-Identifier: GPL-2.0
/*
 * Reboot reasons for LK on MT8163 + MT6323, as the stock kernel's hal_rtc_mark_mode() sets them:
 * RTC_PDN1 (PMIC 0x802c) recovery = bits 5:4 = 01, fastboot = bit 13, kpoc = bit 14, committed by
 * RTC_WRTGR (0x803c) = 1 and a wait for BBPU (0x8000) bit 6 (CBUSY), after the PROT unlock
 * (0x8036 <- 0x586a, 0x9136) of rtc_writeif_unlock(). Recovered with `p80h re`, see docs/hardware.md.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reboot-mode.h>
#include <linux/regmap.h>

#define RTC_BBPU	0x8000
#define RTC_BBPU_CBUSY	BIT(6)
#define RTC_PDN1	0x802c
#define RTC_PDN1_MODES	(GENMASK(5, 4) | BIT(13) | BIT(14))
#define RTC_PROT	0x8036
#define RTC_WRTGR	0x803c

struct p80h_reboot_mode {
	struct reboot_mode_driver rm;
	struct regmap *map;
};

static void rtc_commit(struct regmap *map)
{
	unsigned int bbpu;
	int i;

	regmap_write(map, RTC_WRTGR, 1);
	for (i = 0; i < 1000; i++) {
		if (regmap_read(map, RTC_BBPU, &bbpu) || !(bbpu & RTC_BBPU_CBUSY))
			return;
		udelay(10);
	}
}

static int p80h_reboot_mode_write(struct reboot_mode_driver *rm, unsigned int magic)
{
	struct regmap *map = container_of(rm, struct p80h_reboot_mode, rm)->map;

	regmap_write(map, RTC_PROT, 0x586a);
	rtc_commit(map);
	regmap_write(map, RTC_PROT, 0x9136);
	rtc_commit(map);
	regmap_update_bits(map, RTC_PDN1, RTC_PDN1_MODES, magic & RTC_PDN1_MODES);
	rtc_commit(map);
	return 0;
}

static int p80h_reboot_mode_probe(struct platform_device *pdev)
{
	struct p80h_reboot_mode *p;
	struct device_node *np;
	struct platform_device *pmic;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	np = of_parse_phandle(pdev->dev.of_node, "mediatek,pmic", 0);
	if (!np)
		return -EINVAL;
	pmic = of_find_device_by_node(np);
	of_node_put(np);
	if (!pmic || !pmic->dev.parent)
		return -EPROBE_DEFER;
	p->map = dev_get_regmap(pmic->dev.parent, NULL);	/* the PMIC talks through the pwrap regmap */
	put_device(&pmic->dev);
	if (!p->map)
		return -EPROBE_DEFER;
	p->rm.dev = &pdev->dev;
	p->rm.write = p80h_reboot_mode_write;
	return devm_reboot_mode_register(&pdev->dev, &p->rm);
}

static const struct of_device_id p80h_reboot_mode_of_match[] = {
	{ .compatible = "teclast,p80h-reboot-mode" },
	{ }
};

static struct platform_driver p80h_reboot_mode_driver = {
	.probe = p80h_reboot_mode_probe,
	.driver = {
		.name = "p80h-reboot-mode",
		.of_match_table = p80h_reboot_mode_of_match,
	},
};
module_platform_driver(p80h_reboot_mode_driver);
MODULE_LICENSE("GPL");
