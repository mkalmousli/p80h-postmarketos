/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CLK_MT8163_H
#define __CLK_MT8163_H

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"

extern const struct mtk_fixed_factor mt8163_top_divs[], mt8163_infra_divs[];
extern const struct mtk_mux mt8163_top_muxes_a[], mt8163_top_muxes_b[], mt8163_top_muxes_c[];
extern const struct mtk_composite mt8163_infra_muxes[];
extern const struct mtk_gate mt8163_infra_gates[];
extern const size_t mt8163_top_divs_num, mt8163_infra_divs_num, mt8163_top_muxes_a_num,
	mt8163_top_muxes_b_num, mt8163_top_muxes_c_num, mt8163_infra_muxes_num, mt8163_infra_gates_num;

#endif
