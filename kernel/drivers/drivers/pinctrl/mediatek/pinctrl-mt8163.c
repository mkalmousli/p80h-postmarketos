// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 pinctrl. Tables ported from the vendor driver (Copyright (c) 2015 MediaTek Inc.,
 * Hongzhou Yang, Yingjoe Chen; GPL-2.0), devdata laid out like mainline pinctrl-mt8127.c.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <dt-bindings/pinctrl/mt65xx.h>

#include "pinctrl-mtk-common.h"

static const struct mtk_desc_pin mtk_pins_mt8163[] = {
#include "mt8163/pins-0.inc"
#include "mt8163/pins-1.inc"
#include "mt8163/pins-2.inc"
#include "mt8163/pins-3.inc"
#include "mt8163/pins-4.inc"
#include "mt8163/pins-5.inc"
#include "mt8163/pins-6.inc"
#include "mt8163/pins-7.inc"
#include "mt8163/pins-8.inc"
#include "mt8163/pins-9.inc"
#include "mt8163/pins-10.inc"
#include "mt8163/pins-11.inc"
#include "mt8163/pins-12.inc"
#include "mt8163/pins-13.inc"
#include "mt8163/pins-14.inc"
#include "mt8163/pins-15.inc"
};

static const struct mtk_drv_group_desc mt8163_drv_grp[] = {
#include "mt8163/drvgrp-0.inc"
};

static const struct mtk_pin_drv_grp mt8163_pin_drv[] = {
#include "mt8163/pindrv-0.inc"
#include "mt8163/pindrv-1.inc"
};

static const struct mtk_pin_spec_pupd_set_samereg mt8163_spec_pupd[] = {
#include "mt8163/pupd-0.inc"
};

static const struct mtk_pin_ies_smt_set mt8163_ies_set[] = {
#include "mt8163/ies-0.inc"
};

static const struct mtk_pin_ies_smt_set mt8163_smt_set[] = {
#include "mt8163/smt-0.inc"
};

static const struct mtk_pinctrl_devdata mt8163_pinctrl_data = {
	.pins = mtk_pins_mt8163,
	.npins = ARRAY_SIZE(mtk_pins_mt8163),
	.grp_desc = mt8163_drv_grp,
	.n_grp_cls = ARRAY_SIZE(mt8163_drv_grp),
	.pin_drv_grp = mt8163_pin_drv,
	.n_pin_drv_grps = ARRAY_SIZE(mt8163_pin_drv),
	.spec_ies = mt8163_ies_set,
	.n_spec_ies = ARRAY_SIZE(mt8163_ies_set),
	.spec_pupd = mt8163_spec_pupd,
	.n_spec_pupd = ARRAY_SIZE(mt8163_spec_pupd),
	.spec_smt = mt8163_smt_set,
	.n_spec_smt = ARRAY_SIZE(mt8163_smt_set),
	.spec_pull_set = mtk_pctrl_spec_pull_set_samereg,
	.spec_ies_smt_set = mtk_pconf_spec_set_ies_smt_range,
	.dir_offset = 0x0000,
	.pullen_offset = 0x0100,
	.pullsel_offset = 0x0200,
	.dout_offset = 0x0400,
	.din_offset = 0x0500,
	.pinmux_offset = 0x0600,
	.type1_start = 155,
	.type1_end = 155,
	.port_shf = 4,
	.port_mask = 0xf,
	.port_align = 4,
	.mode_mask = 0xf,
	.mode_per_reg = 5,
	.mode_shf = 4,
	.eint_hw = {
		.port_mask = 7,
		.ports     = 6,
		.ap_num    = 169,
		.db_cnt    = 64,
		.db_time   = debounce_time_mt2701,
	},
};

static const struct of_device_id mt8163_pctrl_match[] = {
	{ .compatible = "mediatek,mt8163-pinctrl", .data = &mt8163_pinctrl_data },
	{ }
};

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mtk_pctrl_common_probe,
	.driver = {
		.name = "mediatek-mt8163-pinctrl",
		.of_match_table = mt8163_pctrl_match,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}
arch_initcall(mtk_pinctrl_init);
