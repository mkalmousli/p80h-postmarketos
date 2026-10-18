// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 topckgen muxes (b)
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

static const char * const atb_parents[] = { "clk26m", "syspll1_d2" };


static const char * const aud_intbus_parents[] = { "clk26m", "syspll1_d4", "syspll4_d2" };


static const char * const audio_parents[] = { "clk26m", "syspll3_d4", "syspll4_d4",
	"syspll1_d16" };


static const char * const dpi0_parents[] = { "clk26m", "lvdspll_ck", "lvdspll_d2", "lvdspll_d4",
	"lvdspll_d8" };


static const char * const mjc_parents[] = { "clk26m", "syspll_d5", "univpll_d5" };


static const char * const msdc30_1_parents[] = { "clk26m", "univpll2_d2", "msdcpll_d2",
	"univpll1_d4", "syspll2_d2", "syspll_d7", "univpll_d7" };


static const char * const msdc30_2_parents[] = { "clk26m", "univpll2_d2", "msdcpll_d2",
	"univpll1_d4", "syspll2_d2", "syspll_d7", "univpll_d7" };


static const char * const msdc50_3_h_parents[] = { "clk26m", "syspll1_d2", "syspll2_d2",
	"syspll4_d2" };


static const char * const msdc50_3_parents[] = { "clk26m", "msdcpll_ck", "msdcpll_d2",
	"univpll1_d4", "syspll2_d2", "syspll_d7", "msdcpll_d4", "univpll_d2", "univpll1_d2" };


static const char * const pmicspi_parents[] = { "clk26m", "univpll_d26" };


static const char * const scp_parents[] = { "clk26m", "mpll_208m_ck", "syspll1_d4" };


const struct mtk_mux mt8163_top_muxes_b[] = {
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel", msdc30_1_parents, 0x0070, 0x0074, 0x0078, 8, 3, 15, 0x0004, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL, "msdc30_2_sel", msdc30_2_parents, 0x0070, 0x0074, 0x0078, 16, 3, 23, 0x0004, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_3_HSEL, "msdc50_3_hsel", msdc50_3_h_parents, 0x0080, 0x0084, 0x0088, 0, 2, 7, 0x0004, 15),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_3_SEL, "msdc50_3_sel", msdc50_3_parents, 0x0080, 0x0084, 0x0088, 8, 4, 15, 0x0004, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents, 0x0080, 0x0084, 0x0088, 16, 2, 23, 0x0004, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel", aud_intbus_parents, 0x0080, 0x0084, 0x0088, 24, 2, 31, 0x0004, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PMICSPI_SEL, "pmicspi_sel", pmicspi_parents, 0x0090, 0x0094, 0x0098, 0, 1, 7, 0x0004, 19),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, 0x0090, 0x0094, 0x0098, 8, 2, 15, 0x0004, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, 0x0090, 0x0094, 0x0098, 16, 1, 23, 0x0004, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MJC_SEL, "mjc_sel", mjc_parents, 0x0090, 0x0094, 0x0098, 24, 2, 31, 0x0004, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPI0_SEL, "dpi0_sel", dpi0_parents, 0x00a0, 0x00a4, 0x00a8, 0, 3, 7, 0x0004, 23),
};

const size_t mt8163_top_muxes_b_num = ARRAY_SIZE(mt8163_top_muxes_b);
