// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 topckgen fixed dividers
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

const struct mtk_fixed_factor mt8163_top_divs[] = {
	FACTOR(CLK_TOP_MAIN_H546M, "main_h546m", "mainpll", 1, 2),
	FACTOR(CLK_TOP_MAIN_H364M, "main_h364m", "mainpll", 1, 3),
	FACTOR(CLK_TOP_MAIN_H218P4M, "main_h218p4m", "mainpll", 1, 5),
	FACTOR(CLK_TOP_MAIN_H156M, "main_h156m", "mainpll", 1, 7),

	FACTOR(CLK_TOP_UNIV_624M, "univ_624m", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIV_416M, "univ_416m", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIV_249P6M, "univ_249p6m", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIV_178P3M, "univ_178p3m", "univpll", 1, 7),
	FACTOR(CLK_TOP_UNIV_48M, "univ_48m", "univpll", 1, 26),

	FACTOR(CLK_TOP_APLL1, "apll1_ck", "aud1pll", 1, 1),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "aud2pll", 1, 1),

	FACTOR(CLK_TOP_CLKRTC_EXT, "clkrtc_ext", "clk32k", 1, 1),
	FACTOR(CLK_TOP_CLKRTC_INT, "clkrtc_int", "clk26m", 1, 793),

	FACTOR(CLK_TOP_DMPLL, "dmpll_ck", "clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_MEM_CKGEN_OCC, "f26m_mem_ckgen", "clk26m", 1, 1),


	FACTOR(CLK_TOP_LVDSPLL, "lvdspll_ck", "lvdspll", 1, 1),
	FACTOR(CLK_TOP_LVDSPLL_D2, "lvdspll_d2", "lvdspll", 1, 2),
	FACTOR(CLK_TOP_LVDSPLL_D4, "lvdspll_d4", "lvdspll", 1, 4),
	FACTOR(CLK_TOP_LVDSPLL_D8, "lvdspll_d8", "lvdspll", 1, 8),
	FACTOR(CLK_TOP_LVDSPLL_ETH, "lvdspll_eth_ck", "lvdspll", 1, 4),

	FACTOR(CLK_TOP_MMPLL, "mmpll_ck", "mmpll", 1, 1),

	FACTOR(CLK_TOP_MPLL_208M, "mpll_208m_ck", "mpll", 1, 1),

	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(CLK_TOP_MSDCPLL_D4, "msdcpll_d4", "msdcpll", 1, 4),

	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "main_h546m", 1, 1),
	FACTOR(CLK_TOP_SYSPLL_D2P5, "syspll_d2p5", "main_h218p4m", 2, 1),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "main_h364m", 1, 1),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "main_h218p4m", 1, 1),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "main_h156m", 1, 1),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "main_h546m", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "main_h546m", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "main_h546m", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "main_h546m", 1, 16),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "main_h364m", 1, 2),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "main_h364m", 1, 4),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "main_h364m", 1, 8),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "main_h218p4m", 1, 2),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "main_h218p4m", 1, 4),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "main_h156m", 1, 2),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "main_h156m", 1, 4),

	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll", 1, 16),

	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univ_624m", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D2P5, "univpll_d2p5", "univ_249p6m", 2, 1),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univ_416m", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univ_249p6m", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7", "univ_178p3m", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D26, "univpll_d26", "univ_48m", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univ_624m", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univ_624m", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univ_416m", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univ_416m", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univ_416m", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univ_249p6m", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univ_249p6m", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL3_D8, "univpll3_d8", "univ_249p6m", 1, 8),

	FACTOR(CLK_TOP_VENCPLL, "vencpll_ck", "vencpll", 1, 1),

	FACTOR(CLK_TOP_APLL2_DIV0, "apll2_div0", "aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_APLL2_DIV1, "apll2_div1", "aud_2_sel", 1, 1),
};

const size_t mt8163_top_divs_num = ARRAY_SIZE(mt8163_top_divs);

