// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 USB2 PHY (register block inside the MUSB IP, at 0x11210800).
 *
 * Power-on and recover sequences ported from the vendor MUSB driver
 * (drivers/misc/mediatek/usb20/mt8163/usb20_phy.c, Copyright MediaTek Inc., GPL-2.0).
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

struct mt8163_phy {
	void __iomem *phy, *musb;
	struct delayed_work dbg;
	int n;
};

static void mod8(void __iomem *base, u32 off, u8 set, u8 clr)
{
	writeb((readb(base + off) & ~clr) | set, base + off);
}

static void mt8163_usb_phy_seq(struct mt8163_phy *u)
{
	void __iomem *p = u->phy;

	mod8(p, 0x6b, 0, 0x04);		/* force_uart_en = 0 */
	mod8(p, 0x6e, 0, 0x01);		/* RG_UART_EN = 0 */
	mod8(p, 0x21, 0, 0x03);		/* usb20 gpio ctl/mode = 0 */
	mod8(p, 0x1a, 0, 0x80);		/* BC11 sw_en = 0 */
	mod8(p, 0x22, 0x04, 0x03);	/* dp 100k mode */
	mod8(p, 0x20, 0x10, 0);		/* OTG enable */
	mod8(p, 0x1d, 0, 0x10);		/* recover: */
	mod8(p, 0x68, 0, 0xf4);		/* drop every forced-state bit the bootloader may have left */
	mod8(p, 0x69, 0, 0x3c);
	mod8(p, 0x6a, 0, 0xbe);		/* includes force_suspendm */
	mod8(p, 0x1a, 0x10, 0x80);	/* VBUS comparator on */
	mod8(p, 0x18, 0x06, 0x08);	/* RX sensitivity */
	udelay(800);
	mod8(p, 0x6c, 0, 0x10);		/* force device mode */
	mod8(p, 0x6c, 0x2e, 0);
	mod8(p, 0x6d, 0x3e, 0);
	mod8(p, 0x05, 0x77, 0);		/* VRT/TERM vref */
}

static void mt8163_usb_phy_dbg(struct work_struct *w)
{
	struct mt8163_phy *u = container_of(to_delayed_work(w), struct mt8163_phy, dbg);

	pr_info("p80h-usb: power=%02x devctl=%02x intrusb=%02x phy 68=%02x 6a=%02x 6c=%02x 1a=%02x\n",
		readb(u->musb + 1), readb(u->musb + 0x60), readb(u->musb + 0x0a), readb(u->phy + 0x68),
		readb(u->phy + 0x6a), readb(u->phy + 0x6c), readb(u->phy + 0x1a));
	if (++u->n < 12)
		schedule_delayed_work(&u->dbg, 2 * HZ);
}

static int mt8163_usb_phy_power_on(struct phy *p)
{
	mt8163_usb_phy_seq(phy_get_drvdata(p));
	return 0;
}

static const struct phy_ops mt8163_usb_phy_ops = {
	.power_on = mt8163_usb_phy_power_on,
	.owner = THIS_MODULE,
};

static int mt8163_usb_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt8163_phy *u = devm_kzalloc(dev, sizeof(*u), GFP_KERNEL);
	static const char *const names[] = { "univpll", "icusb", "main", "mcu" };
	struct phy *phy;
	struct clk *clk;
	int i;

	if (!u)
		return -ENOMEM;
	u->phy = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(u->phy))
		return PTR_ERR(u->phy);
	/* the PHY registers live inside the MUSB block: the 48 MHz UTMI clock (univpll) and the bus gates must run first */
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		clk = devm_clk_get_enabled(dev, names[i]);
		if (IS_ERR(clk))
			return dev_err_probe(dev, PTR_ERR(clk), "%s clock\n", names[i]);
	}
	/* the MUSB glue writes its registers before phy_power_on(); the UTMI clock has to run by then */
	mt8163_usb_phy_seq(u);
	u->musb = devm_ioremap(dev, 0x11200000, 0x100);
	INIT_DELAYED_WORK(&u->dbg, mt8163_usb_phy_dbg);
	schedule_delayed_work(&u->dbg, 3 * HZ);
	phy = devm_phy_create(dev, NULL, &mt8163_usb_phy_ops);
	if (IS_ERR(phy))
		return PTR_ERR(phy);
	phy_set_drvdata(phy, u);
	return PTR_ERR_OR_ZERO(devm_of_phy_provider_register(dev, of_phy_simple_xlate));
}

static const struct of_device_id mt8163_usb_phy_ids[] = {
	{ .compatible = "mediatek,mt8163-usb-phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_usb_phy_ids);

static struct platform_driver mt8163_usb_phy_driver = {
	.probe = mt8163_usb_phy_probe,
	.driver = { .name = "phy-mt8163-usb", .of_match_table = mt8163_usb_phy_ids },
};
static int __init mt8163_usb_phy_init(void)
{
	return platform_driver_register(&mt8163_usb_phy_driver);
}
subsys_initcall(mt8163_usb_phy_init);	/* must be bound before the MUSB glue probes */

MODULE_DESCRIPTION("MediaTek MT8163 USB2 PHY");
MODULE_LICENSE("GPL");
