// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 infracfg muxes and dividers
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

const struct mtk_fixed_factor mt8163_infra_divs[] = {
	FACTOR(CLK_INFRA_OSC, "osc_ck", "clk26m", 8, 1),
	FACTOR(CLK_INFRA_OSC_D8, "osc_d8", "osc_ck", 1, 8),
	FACTOR(CLK_INFRA_OSC_D16, "osc_d16", "osc_ck", 1, 16),
	FACTOR(CLK_INFRA_CLK26M_D8, "clk26m_d8", "clk26m", 1, 8),
	FACTOR(CLK_INFRA_ETH_D2, "eth_d2", "eth_sel", 1, 2),
	FACTOR(CLK_INFRA_ONFI_D2, "onfi_d2", "onfi_sel", 1, 2),
	FACTOR(CLK_INFRA_CLK_13M, "clk13m", "clk26m", 1, 2),
};

static const char * const infra_uart_parents[] = { "clk26m", "uart_sel" };

static const char * const infra_spi_parents[] = { "axi_sel", "spi_sel" };

static const char * const infra_dramc_parents[] = { "clk26m", "clk26m_d8" };

static const char * const infra_ulp_parents[] = { "osc_d8", "osc_d16" };

static const char * const infra_eth_parents[] = { "eth_d2", "eth_sel" };

static const char * const infra_nfi_parents[] = { "axi_sel", "onfi_d2" };

const struct mtk_composite mt8163_infra_muxes[] = {
	MUX(CLK_INFRA_UART0_SEL, "infra_uart0_sel", infra_uart_parents, 0x098, 0, 1),
	MUX(CLK_INFRA_UART1_SEL, "infra_uart1_sel", infra_uart_parents, 0x098, 1, 1),
	MUX(CLK_INFRA_UART2_SEL, "infra_uart2_sel", infra_uart_parents, 0x098, 2, 1),
	MUX(CLK_INFRA_UART3_SEL, "infra_uart3_sel", infra_uart_parents, 0x098, 3, 1),
	MUX(CLK_INFRA_SPI_SEL, "infra_spi_sel", infra_spi_parents, 0x098, 4, 1),
	MUX(CLK_INFRA_DRAMC_SEL, "infra_dramc_sel", infra_dramc_parents, 0x098, 7, 1),
	MUX(CLK_INFRA_ULPOSC_SEL, "infra_ulp_sel", infra_ulp_parents, 0x098, 8, 1),
	MUX(CLK_INFRA_ETH_25M_SEL, "infra_eth_sel", infra_eth_parents, 0x098, 9, 1),
	MUX(CLK_INFRA_NFI_SEL, "infra_nfi_sel", infra_nfi_parents, 0x098, 10, 1),
};
const size_t mt8163_infra_divs_num = ARRAY_SIZE(mt8163_infra_divs);
const size_t mt8163_infra_muxes_num = ARRAY_SIZE(mt8163_infra_muxes);
