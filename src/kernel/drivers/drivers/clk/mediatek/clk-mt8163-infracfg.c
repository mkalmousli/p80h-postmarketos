// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 infracfg clocks and resets, ported from the vendor clk-mt8163.c
 * (MediaTek Inc., GPL-2.0) onto the mainline simple-probe framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#include "reset.h"
#include "clk-mt8163.h"

static DEFINE_SPINLOCK(mt8163_infra_lock);
static u16 infra_rst_ofs[] = { 0x30, 0x34 };

static const struct mtk_clk_rst_desc infra_rst_desc = {
	.version = MTK_RST_SIMPLE,
	.rst_bank_ofs = infra_rst_ofs,
	.rst_bank_nr = ARRAY_SIZE(infra_rst_ofs),
};

/* the tables live in other files, so their sizes are only known at run time */
static struct mtk_clk_desc infra_desc = {
	.clks = mt8163_infra_gates,
	.composite_clks = mt8163_infra_muxes,
	.factor_clks = mt8163_infra_divs,
	.rst_desc = &infra_rst_desc,
	.clk_lock = &mt8163_infra_lock,
};

static const struct of_device_id mt8163_infracfg_ids[] = {
	{ .compatible = "mediatek,mt8163-infracfg", .data = &infra_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_infracfg_ids);

static struct platform_driver mt8163_infracfg_driver = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = { .name = "clk-mt8163-infracfg", .of_match_table = mt8163_infracfg_ids },
};

static int __init mt8163_infracfg_init(void)
{
	infra_desc.num_clks = mt8163_infra_gates_num;
	infra_desc.num_composite_clks = mt8163_infra_muxes_num;
	infra_desc.num_factor_clks = mt8163_infra_divs_num;
	return platform_driver_register(&mt8163_infracfg_driver);
}
subsys_initcall(mt8163_infracfg_init);

MODULE_DESCRIPTION("MediaTek MT8163 infracfg clocks");
MODULE_LICENSE("GPL");
