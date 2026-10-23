// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 mmsys clock gates, instantiated by mtk-mmsys ("clk-mt8163-mm").
 * Bit layout from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0);
 * it differs from MT8167 (OVL1 sits at MM0 bit 11). Confirmed against the gates LK leaves open
 * (CG_CON0 0xffe96bfc: SMI, LARB0, OVL0, RDMA0, COLOR, AAL, GAMMA, UFOE).
 */
#include <linux/clk-provider.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"

static const struct mtk_gate_regs mm0_cg_regs = { .set_ofs = 0x104, .clr_ofs = 0x108, .sta_ofs = 0x100 };
static const struct mtk_gate_regs mm1_cg_regs = { .set_ofs = 0x114, .clr_ofs = 0x118, .sta_ofs = 0x110 };

#define GATE_MM0(_id, _name, _parent, _shift)	\
	GATE_MTK(_id, _name, _parent, &mm0_cg_regs, _shift, &mtk_clk_gate_ops_setclr)
#define GATE_MM1(_id, _name, _parent, _shift)	\
	GATE_MTK(_id, _name, _parent, &mm1_cg_regs, _shift, &mtk_clk_gate_ops_setclr)
/* the display engines fetch through SMI/larb0, and no SMI driver owns these yet */
#define GATE_MM0_CRIT(_id, _name, _parent, _shift)	\
	GATE_MTK_FLAGS(_id, _name, _parent, &mm0_cg_regs, _shift, &mtk_clk_gate_ops_setclr, CLK_IS_CRITICAL)

/* every CLK_MM_* id 0..CLK_MM_NR_CLK-1 in order: the clk-mtk table is sized by entry count */
static const struct mtk_gate mm_clks[] = {
	GATE_MM0_CRIT(CLK_MM_SMI_COMMON, "mm_smi_common", "mm_sel", 0),
	GATE_MM0_CRIT(CLK_MM_SMI_LARB0, "mm_smi_larb0", "mm_sel", 1),
	GATE_MM0(CLK_MM_CAM_MDP, "mm_cam_mdp", "mm_sel", 2),
	GATE_MM0(CLK_MM_MDP_RDMA, "mm_mdp_rdma", "mm_sel", 3),
	GATE_MM0(CLK_MM_MDP_RSZ0, "mm_mdp_rsz0", "mm_sel", 4),
	GATE_MM0(CLK_MM_MDP_RSZ1, "mm_mdp_rsz1", "mm_sel", 5),
	GATE_MM0(CLK_MM_MDP_TDSHP, "mm_mdp_tdshp", "mm_sel", 6),
	GATE_MM0(CLK_MM_MDP_WDMA, "mm_mdp_wdma", "mm_sel", 7),
	GATE_MM0(CLK_MM_MDP_WROT, "mm_mdp_wrot", "mm_sel", 8),
	GATE_MM0(CLK_MM_FAKE_ENG, "mm_fake_eng", "mm_sel", 9),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0", "mm_sel", 10),
	GATE_MM0(CLK_MM_DISP_OVL1, "mm_disp_ovl1", "mm_sel", 11),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0", "mm_sel", 12),
	GATE_MM0(CLK_MM_DISP_RDMA1, "mm_disp_rdma1", "mm_sel", 13),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0", "mm_sel", 14),
	GATE_MM0(CLK_MM_DISP_COLOR, "mm_disp_color", "mm_sel", 15),
	GATE_MM0(CLK_MM_DISP_CCORR, "mm_disp_ccorr", "mm_sel", 16),
	GATE_MM0(CLK_MM_DISP_AAL, "mm_disp_aal", "mm_sel", 17),
	GATE_MM0(CLK_MM_DISP_GAMMA, "mm_disp_gamma", "mm_sel", 18),
	GATE_MM0(CLK_MM_DISP_DITHER, "mm_disp_dither", "mm_sel", 19),
	GATE_MM0(CLK_MM_DISP_UFOE, "mm_disp_ufoe", "mm_sel", 20),
	GATE_MM0(CLK_MM_LARB4_AXI_ASIF_MM, "mm_larb4_mm", "mm_sel", 21),
	GATE_MM0(CLK_MM_LARB4_AXI_ASIF_MJC, "mm_larb4_mjc", "mm_sel", 22),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1", "mm_sel", 23),
	GATE_MM0(CLK_MM_UFOD_RDMA0_L0, "mm_ufodrdma0_l0", "mm_sel", 24),
	GATE_MM0(CLK_MM_UFOD_RDMA0_L1, "mm_ufodrdma0_l1", "mm_sel", 25),
	GATE_MM0(CLK_MM_UFOD_RDMA0_L2, "mm_ufodrdma0_l2", "mm_sel", 26),
	GATE_MM0(CLK_MM_UFOD_RDMA0_L3, "mm_ufodrdma0_l3", "mm_sel", 27),
	GATE_MM0(CLK_MM_UFOD_RDMA1_L0, "mm_ufodrdma1_l0", "mm_sel", 28),
	GATE_MM0(CLK_MM_UFOD_RDMA1_L1, "mm_ufodrdma1_l1", "mm_sel", 29),
	GATE_MM0(CLK_MM_UFOD_RDMA1_L2, "mm_ufodrdma1_l2", "mm_sel", 30),
	GATE_MM0(CLK_MM_UFOD_RDMA1_L3, "mm_ufodrdma1_l3", "mm_sel", 31),
	GATE_MM1(CLK_MM_DISP_PWM_MM, "mm_disp_pwm_mm", "mm_sel", 0),
	GATE_MM1(CLK_MM_DISP_PWM_26M, "mm_disp_pwm_26m", "clk26m", 1),
	GATE_MM1(CLK_MM_DSI_ENGINE, "mm_dsi_engine", "mm_sel", 2),
	GATE_MM1(CLK_MM_DSI_DIGITAL, "mm_dsi_digital", "clk26m", 3),
	GATE_MM1(CLK_MM_DPI0_PIXEL, "mm_dpi0_pixel", "mm_sel", 4),
	GATE_MM1(CLK_MM_DPI0_ENGINE, "mm_dpi0_engine", "mm_sel", 5),
	GATE_MM1(CLK_MM_LVDS_PIXEL, "mm_lvds_pixel", "mm_sel", 6),
	GATE_MM1(CLK_MM_LVDS_CTS, "mm_lvds_cts", "clk26m", 7),
	GATE_MM1(CLK_MM_DPI1_PIXEL, "mm_dpi1_pixel", "mm_sel", 8),
	GATE_MM1(CLK_MM_DPI1_ENGINE, "mm_dpi1_engine", "mm_sel", 9),
	GATE_MM1(CLK_MM_HDMI_PIXEL, "mm_hdmi_pixel", "mm_sel", 10),
	GATE_MM1(CLK_MM_HDMI_SPDIF, "mm_hdmi_spdif", "clk26m", 11),
	GATE_MM1(CLK_MM_HDMI_ADSP, "mm_hdmi_audio", "clk26m", 12),
	GATE_MM1(CLK_MM_HDMI_PLLCK, "mm_hdmi_pllck", "clk26m", 13),
	GATE_MM1(CLK_MM_DISP_DSC_ENGINE, "mm_disp_dsc_eng", "mm_sel", 14),
	GATE_MM1(CLK_MM_DISP_DSC_MEM, "mm_disp_dsc_mem", "mm_sel", 15),
};

static const struct mtk_clk_desc mm_desc = {
	.clks = mm_clks,
	.num_clks = ARRAY_SIZE(mm_clks),
};

static const struct platform_device_id clk_mt8163_mm_id_table[] = {
	{ .name = "clk-mt8163-mm", .driver_data = (kernel_ulong_t)&mm_desc },
	{ }
};

static struct platform_driver clk_mt8163_mm_drv = {
	.probe = mtk_clk_pdev_probe,
	.remove = mtk_clk_pdev_remove,
	.driver = { .name = "clk-mt8163-mm" },
	.id_table = clk_mt8163_mm_id_table,
};
builtin_platform_driver(clk_mt8163_mm_drv);
