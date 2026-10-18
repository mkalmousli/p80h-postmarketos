// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 apmixedsys PLLs. PLL table ported from the vendor clk-mt8163.c
 * (MediaTek Inc., GPL-2.0); probe modelled on the mainline MT8173 driver.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "clk-mtk.h"
#include "clk-pll.h"

#define REGOFF_REF2USB		0x8
#define MT8163_PLL_FMAX		(2500UL * MHZ)
#define CON0_MT8163_RST_BAR	BIT(24)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en, _flags, _pcwbits, _pd_reg, _pd_shift, _tuner_reg,	\
	      _pcw_reg, _div_table) {								\
		.id = _id, .name = _name, .reg = _reg, .pwr_reg = _pwr_reg, .en_mask = _en,	\
		.flags = _flags, .rst_bar_mask = CON0_MT8163_RST_BAR, .fmax = MT8163_PLL_FMAX,	\
		.pcwbits = _pcwbits, .pd_reg = _pd_reg, .pd_shift = _pd_shift,			\
		.tuner_reg = _tuner_reg, .pcw_reg = _pcw_reg, .pcw_shift = 0,			\
		.div_table = _div_table,							\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en, _flags, _pcwbits, _pd_reg, _pd_shift, _tuner_reg, _pcw_reg) \
	PLL_B(_id, _name, _reg, _pwr_reg, _en, _flags, _pcwbits, _pd_reg, _pd_shift, _tuner_reg, _pcw_reg, NULL)

static const struct mtk_pll_div_table mmpll_div_table[] = {
	{ .div = 0, .freq = MT8163_PLL_FMAX },
	{ .div = 1, .freq = 1000000000 },
	{ .div = 2, .freq = 625000000 },
	{ .div = 3, .freq = 253500000 },
	{ .div = 4, .freq = 126750000 },
	{ }
};

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x220, 0x22c, 0x1, HAVE_RST_BAR, 21, 0x220, 4, 0x0, 0x224),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", 0x230, 0x23c, 0x1, HAVE_RST_BAR, 21, 0x230, 4, 0x0, 0x234),
	PLL_B(CLK_APMIXED_MMPLL, "mmpll", 0x240, 0x24c, 0x1, 0, 21, 0x244, 24, 0x0, 0x244, mmpll_div_table),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x250, 0x25c, 0x1, 0, 21, 0x250, 4, 0x0, 0x254),
	PLL(CLK_APMIXED_VENCPLL, "vencpll", 0x260, 0x26c, 0x1, 0, 21, 0x260, 4, 0x0, 0x264),
	PLL(CLK_APMIXED_TVDPLL, "tvdpll", 0x270, 0x27c, 0x1, 0, 21, 0x270, 4, 0x0, 0x274),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x280, 0x28c, 0x1, 0, 21, 0x280, 4, 0x0, 0x284),
	PLL(CLK_APMIXED_AUD1PLL, "aud1pll", 0x2a0, 0x2ac, 0x1, 0, 31, 0x2a0, 4, 0x300, 0x2a4),
	PLL(CLK_APMIXED_AUD2PLL, "aud2pll", 0x2b0, 0x2bc, 0x1, 0, 31, 0x2b0, 4, 0x304, 0x2b4),
	PLL(CLK_APMIXED_LVDSPLL, "lvdspll", 0x2c0, 0x2cc, 0x1, 0, 21, 0x2c0, 4, 0x0, 0x2c4),
};

/* LK leaves most PLLs running; re-running the power-on sequence on a live PLL stalls the bus */
static int mt8163_pll_prepare(struct clk_hw *hw)
{
	return mtk_pll_ops.is_prepared(hw) ? 0 : mtk_pll_ops.prepare(hw);
}

static int mt8163_apmixed_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_hw_onecell_data *data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);
	void __iomem *base = of_iomap(node, 0);
	struct clk_hw *hw;
	static struct clk_ops ops;
	int i;

	if (!data || !base)
		return -ENOMEM;
	ops = mtk_pll_ops;
	ops.prepare = mt8163_pll_prepare;
	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		struct mtk_clk_pll *pll = kzalloc(sizeof(*pll), GFP_KERNEL);

		hw = pll ? mtk_clk_register_pll_ops(pll, &plls[i], base, &ops) : ERR_PTR(-ENOMEM);
		if (IS_ERR(hw))
			return PTR_ERR(hw);
		data->hws[plls[i].id] = hw;
	}
	hw = mtk_clk_register_ref2usb_tx("ref2usb_tx", "clk26m", base + REGOFF_REF2USB);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	data->hws[CLK_APMIXED_REF2USB_TX] = hw;
	return of_clk_add_hw_provider(node, of_clk_hw_onecell_get, data);
}

static const struct of_device_id mt8163_apmixed_ids[] = {
	{ .compatible = "mediatek,mt8163-apmixedsys" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_apmixed_ids);

static struct platform_driver mt8163_apmixed_driver = {
	.probe = mt8163_apmixed_probe,
	.driver = { .name = "clk-mt8163-apmixedsys", .of_match_table = mt8163_apmixed_ids },
};
module_platform_driver(mt8163_apmixed_driver);

MODULE_DESCRIPTION("MediaTek MT8163 apmixedsys PLLs");
MODULE_LICENSE("GPL");
