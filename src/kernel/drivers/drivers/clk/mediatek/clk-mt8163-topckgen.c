// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 topckgen clocks. Probe modelled on the mainline MT8173/MT8167 drivers; the
 * clock tables are ported from the vendor clk-mt8163.c (MediaTek Inc., GPL-2.0).
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

static DEFINE_SPINLOCK(mt8163_top_lock);

static int mt8163_topckgen_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_hw_onecell_data *data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	int r;

	if (!data)
		return -ENOMEM;
	pr_info("p80h: topckgen factors\n");
	r = mtk_clk_register_factors(mt8163_top_divs, mt8163_top_divs_num, data);
	if (r)
		return r;
	pr_info("p80h: topckgen muxes a\n");
	r = mtk_clk_register_muxes(&pdev->dev, mt8163_top_muxes_a, mt8163_top_muxes_a_num, node,
				   &mt8163_top_lock, data);
	if (!r)
		pr_info("p80h: topckgen muxes b\n");
		r = mtk_clk_register_muxes(&pdev->dev, mt8163_top_muxes_b, mt8163_top_muxes_b_num,
					   node, &mt8163_top_lock, data);
	if (!r)
		pr_info("p80h: topckgen muxes c\n");
		r = mtk_clk_register_muxes(&pdev->dev, mt8163_top_muxes_c, mt8163_top_muxes_c_num,
					   node, &mt8163_top_lock, data);
	if (r)
		return r;
	return of_clk_add_hw_provider(node, of_clk_hw_onecell_get, data);
}

static const struct of_device_id mt8163_topckgen_ids[] = {
	{ .compatible = "mediatek,mt8163-topckgen" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_topckgen_ids);

static struct platform_driver mt8163_topckgen_driver = {
	.probe = mt8163_topckgen_probe,
	.driver = { .name = "clk-mt8163-topckgen", .of_match_table = mt8163_topckgen_ids },
};
module_platform_driver(mt8163_topckgen_driver);

MODULE_DESCRIPTION("MediaTek MT8163 topckgen clocks");
MODULE_LICENSE("GPL");
