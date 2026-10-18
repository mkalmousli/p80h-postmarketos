// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8163 infracfg gates
 * Tables ported from the vendor clk-mt8163.c (Copyright (c) 2015 MediaTek Inc., James Liao, GPL-2.0)
 * for the mainline clk-mtk framework.
 */
#include <dt-bindings/clock/mt8163-clk.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt8163.h"

static const struct mtk_gate_regs infra0_cg_regs = {
	.set_ofs = 0x0080,
	.clr_ofs = 0x0084,
	.sta_ofs = 0x0090,
};

static const struct mtk_gate_regs infra1_cg_regs = {
	.set_ofs = 0x0088,
	.clr_ofs = 0x008c,
	.sta_ofs = 0x0094,
};
#define GATE_ICG0(_id, _name, _parent, _shift)	\
	GATE_MTK(_id, _name, _parent, &infra0_cg_regs, _shift, &mtk_clk_gate_ops_setclr)
#define GATE_ICG1(_id, _name, _parent, _shift)	\
	GATE_MTK(_id, _name, _parent, &infra1_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

const struct mtk_gate mt8163_infra_gates[] = {
	/* INFRA0 */
	GATE_ICG0(CLK_INFRA_PMIC_TMR, "infra_pmic_tmr", "clk26m", 0),
	GATE_ICG0(CLK_INFRA_PMIC_AP, "infra_pmic_ap", "clk26m", 1),
	GATE_ICG0(CLK_INFRA_PMIC_MD, "infra_pmic_md", "clk26m", 2),
	GATE_ICG0(CLK_INFRA_PMIC_CONN, "infra_pmic_conn", "clk26m", 3),
	GATE_ICG0(CLK_INFRA_SCPSYS, "infra_scpsys", "scp_sel", 4),
	GATE_ICG0(CLK_INFRA_SEJ, "infra_sej", "axi_sel", 5),
	GATE_ICG0(CLK_INFRA_APXGPT, "infra_apxgpt", "axi_sel", 6),
	GATE_ICG0(CLK_INFRA_USB, "infra_usb", "axi_sel", 7),
	GATE_ICG0(CLK_INFRA_ICUSB, "infra_icusb", "axi_sel", 8),
	GATE_ICG0(CLK_INFRA_GCE, "infra_gce", "axi_sel", 9),
	GATE_ICG0(CLK_INFRA_THERM, "infra_therm", "axi_sel", 10),
	GATE_ICG0(CLK_INFRA_I2C0, "infra_i2c0", "axi_sel", 11),
	GATE_ICG0(CLK_INFRA_I2C1, "infra_i2c1", "axi_sel", 12),
	GATE_ICG0(CLK_INFRA_I2C2, "infra_i2c2", "axi_sel", 13),
	GATE_ICG0(CLK_INFRA_PWM_HCLK, "infra_pwm_hclk", "axi_sel", 15),
	GATE_ICG0(CLK_INFRA_PWM1, "infra_pwm1", "axi_sel", 16),
	GATE_ICG0(CLK_INFRA_PWM2, "infra_pwm2", "axi_sel", 17),
	GATE_ICG0(CLK_INFRA_PWM3, "infra_pwm3", "axi_sel", 18),
	GATE_ICG0(CLK_INFRA_PWM, "infra_pwm", "axi_sel", 21),
	GATE_ICG0(CLK_INFRA_UART0, "infra_uart0", "uart_sel", 22),
	GATE_ICG0(CLK_INFRA_UART1, "infra_uart1", "uart_sel", 23),
	GATE_ICG0(CLK_INFRA_UART2, "infra_uart2", "uart_sel", 24),
	GATE_ICG0(CLK_INFRA_UART3, "infra_uart3", "uart_sel", 25),
	GATE_ICG0(CLK_INFRA_USB_MCU, "infra_usb_mcu", "axi_sel", 26),
	GATE_ICG0(CLK_INFRA_NFI_ECC_66M, "infra_nfi_ecc66", "axi_sel", 27),
	GATE_ICG0(CLK_INFRA_NFI_66M, "infra_nfi_66m", "axi_sel", 28),
	GATE_ICG0(CLK_INFRA_BTIF, "infra_btif", "axi_sel", 31),
	/* INFRA1 */
	GATE_ICG1(CLK_INFRA_SPI, "infra_spi", "spi_sel", 1),
	GATE_ICG1(CLK_INFRA_MSDC3, "infra_msdc3", "msdc50_3_sel", 2),
	GATE_ICG1(CLK_INFRA_MSDC1, "infra_msdc1", "msdc30_1_sel", 4),
	GATE_ICG1(CLK_INFRA_MSDC2, "infra_msdc2", "msdc30_2_sel", 5),
	GATE_ICG1(CLK_INFRA_MSDC0, "infra_msdc0", "msdc30_0_sel", 6),
	GATE_ICG1(CLK_INFRA_GCPU, "infra_gcpu", "axi_sel", 8),
	GATE_ICG1(CLK_INFRA_AUXADC, "infra_auxadc", "clk26m", 10),
	GATE_ICG1(CLK_INFRA_CPUM, "infra_cpum", "axi_sel", 11),
	GATE_ICG1(CLK_INFRA_IRRX, "infra_irrx", "rtc_sel", 12),
	GATE_ICG1(CLK_INFRA_UFO, "infra_ufo", "axi_sel", 13),
	GATE_ICG1(CLK_INFRA_CEC, "infra_cec", "rtc_sel", 14),
	GATE_ICG1(CLK_INFRA_CEC_26M, "infra_cec_26m", "clk26m", 15),
	GATE_ICG1(CLK_INFRA_NFI_BCLK, "infra_nfi_bclk", "spi_sel", 16),
	GATE_ICG1(CLK_INFRA_NFI_ECC, "infra_nfi_ecc", "spi_sel", 17),
	GATE_ICG1(CLK_INFRA_AP_DMA, "infra_ap_dma", "axi_sel", 18),
	GATE_ICG1(CLK_INFRA_XIU, "infra_xiu", "axi_sel", 19),
	GATE_ICG1(CLK_INFRA_DEVICE_APC, "infra_devapc", "axi_sel", 20),
	GATE_ICG1(CLK_INFRA_XIU2AHB, "infra_xiu2ahb", "axi_sel", 21),
	GATE_ICG1(CLK_INFRA_L2C_SRAM, "infra_l2c_sram", "axi_sel", 22),
	GATE_ICG1(CLK_INFRA_ETH_50M, "infra_eth_50m", "eth_sel", 23),
	GATE_ICG1(CLK_INFRA_DEBUGSYS, "infra_debugsys", "axi_sel", 24),
	GATE_ICG1(CLK_INFRA_AUDIO, "infra_audio", "axi_sel", 25),
	GATE_ICG1(CLK_INFRA_ETH_25M, "infra_eth_25m", "eth_sel", 26),
	GATE_ICG1(CLK_INFRA_NFI, "infra_nfi", "axi_sel", 27),
	GATE_ICG1(CLK_INFRA_ONFI, "infra_onfi", "onfi_sel", 28),
	GATE_ICG1(CLK_INFRA_SNFI, "infra_snfi", "snfi_sel", 29),
	GATE_ICG1(CLK_INFRA_ETH, "infra_eth", "axi_sel", 30),
	GATE_ICG1(CLK_INFRA_DRAMC_F26M, "infra_dramc_26m", "clk26m", 31),
};
const size_t mt8163_infra_gates_num = ARRAY_SIZE(mt8163_infra_gates);
