// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 topckgen muxes (a)
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

static const char * const axi_parents[] = { "clk26m", "syspll1_d4" };


static const char * const camtg_parents[] = { "clk26m", "univpll_d26", "univpll2_d2" };


static const char * const ddrphycfg_parents[] = { "clk26m", "syspll1_d8" };


static const char * const mem_parents[] = { "clk26m", "dmpll_ck", "f26m_mem_ckgen" };


static const char * const mfg_parents[] = { "clk26m", "mmpll_ck", "univpll_d3", "syspll_d3" };


static const char * const mm_parents[] = { "clk26m", "syspll_d3", "univpll_d3", "vencpll_ck" };


static const char * const msdc30_0_parents[] = { "clk26m", "univpll2_d2", "msdcpll_d2",
	"univpll1_d4", "syspll2_d2", "syspll_d7", "univpll_d7" };


static const char * const pwm_parents[] = { "clk26m", "univpll2_d2", "univpll2_d4" };


static const char * const spi_parents[] = { "clk26m", "syspll3_d2" };


static const char * const uart_parents[] = { "clk26m", "univpll2_d8" };


static const char * const vdec_parents[] = { "clk26m", "syspll_d3", "syspll1_d2", "univpll1_d2",
	"syspll1_d4" };


const struct mtk_mux mt8163_top_muxes_a[] = {
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents, 0x0040, 0x0044, 0x0048, 0, 1, 7, 0x0004, 0,
		CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SEL, "mem_sel", mem_parents, 0x0040, 0x0044, 0x0048, 8, 2, 15, 0x0004, 1,
		CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", ddrphycfg_parents, 0x0040, 0x0044, 0x0048, 16, 1, 23, 0x0004, 2,
		CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, 0x0040, 0x0044, 0x0048, 24, 2, 31, 0x0004, 3),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, 0x0050, 0x0054, 0x0058, 0, 2, 7, 0x0004, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VDEC_SEL, "vdec_sel", vdec_parents, 0x0050, 0x0054, 0x0058, 8, 3, 15, 0x0004, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, 0x0050, 0x0054, 0x0058, 24, 2, 31, 0x0004, 7),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel", camtg_parents, 0x0060, 0x0064, 0x0068, 0, 2, 7, 0x0004, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents, 0x0060, 0x0064, 0x0068, 8, 1, 15, 0x0004, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, 0x0060, 0x0064, 0x0068, 16, 1, 23, 0x0004, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_0_SEL, "msdc30_0_sel", msdc30_0_parents, 0x0070, 0x0074, 0x0078, 0, 3, 7, 0x0004, 12),
};

const size_t mt8163_top_muxes_a_num = ARRAY_SIZE(mt8163_top_muxes_a);
