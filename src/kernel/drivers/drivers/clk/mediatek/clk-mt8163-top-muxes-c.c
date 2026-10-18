// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 topckgen muxes (c)
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

static const char * const aud_1_parents[] = { "clk26m", "apll1_ck" };


static const char * const aud_2_parents[] = { "clk26m", "apll2_ck" };


static const char * const dpi1_parents[] = { "clk26m", "tvdpll_d2", "tvdpll_d4", "tvdpll_d8",
	"tvdpll_d16" };


static const char * const eth_parents[] = { "clk26m", "syspll3_d4", "univpll2_d8",
	"lvdspll_eth_ck", "univpll_d26", "syspll2_d8", "syspll4_d4", "univpll3_d8", "clk26m" };


static const char * const onfi_parents[] = { "clk26m", "syspll2_d2", "syspll_d7", "univpll3_d2",
	"syspll2_d4", "univpll3_d4", "syspll4_d4" };


static const char * const rtc_parents[] = { "clkrtc_int", "clkrtc_ext", "clk26m", "univpll3_d8" };

static const char * const scam_parents[] = { "clk26m", "syspll3_d2" };


static const char * const snfi_parents[] = { "clk26m", "univpll2_d8", "univpll3_d4",
	"syspll4_d2", "univpll2_d4", "univpll3_d2", "syspll1_d4", "univpll1_d4", "clk26m" };


static const char * const ufodec_parents[] = { "clk26m", "syspll_d2", "univpll_d2p5",
	"syspll_d2p5", "univpll_d3", "syspll_d3", "syspll1_d2" };


static const char * const ufoenc_parents[] = { "clk26m", "univpll_d2", "syspll_d2",
	"univpll_d2p5", "syspll_d2p5", "univpll_d3", "syspll_d3", "syspll1_d2" };


const struct mtk_mux mt8163_top_muxes_c[] = {
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCAM_SEL, "scam_sel", scam_parents, 0x00a0, 0x00a4, 0x00a8, 8, 1, 15, 0x0004, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents, 0x00a0, 0x00a4, 0x00a8, 16, 1, 23, 0x0004, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL, "aud_2_sel", aud_2_parents, 0x00a0, 0x00a4, 0x00a8, 24, 1, 31, 0x0004, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPI1_SEL, "dpi1_sel", dpi1_parents, 0x00b0, 0x00b4, 0x00b8, 0, 3, 7, 0x0004, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFOENC_SEL, "ufoenc_sel", ufoenc_parents, 0x00b0, 0x00b4, 0x00b8, 8, 3, 15, 0x0004, 27),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFODEC_SEL, "ufodec_sel", ufodec_parents, 0x00b0, 0x00b4, 0x00b8, 16, 3, 23, 0x0004, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_SEL, "eth_sel", eth_parents, 0x00b0, 0x00b4, 0x00b8, 24, 4, 31, 0x0004, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ONFI_SEL, "onfi_sel", onfi_parents, 0x00c0, 0x00c4, 0x00c8, 0, 3, 7, 0x0004, 30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SNFI_SEL, "snfi_sel", snfi_parents, 0x00c0, 0x00c4, 0x00c8, 8, 2, 15, 0x0004, 31),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_RTC_SEL, "rtc_sel", rtc_parents, 0x00c0, 0x00c4, 0x00c8, 24, 2, 31, 0x0008, 0,
		CLK_SET_RATE_PARENT),
};

const size_t mt8163_top_muxes_c_num = ARRAY_SIZE(mt8163_top_muxes_c);
