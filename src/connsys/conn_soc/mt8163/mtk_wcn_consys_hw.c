/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[WMT-CONSYS-HW]"

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include "osal_typedef.h"
#include "mtk_wcn_consys_hw.h"
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#if CONSYS_EMI_MPU_SETTING
#endif

#include <linux/regulator/consumer.h>
#ifdef CONFIG_MTK_HIBERNATION
#endif

#include <linux/of_reserved_mem.h>

#if CONSYS_CLOCK_BUF_CTRL
#endif

#include <linux/pm_runtime.h>
#include <linux/reset.h>

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static INT32 mtk_wmt_probe(struct platform_device *pdev);
static void mtk_wmt_remove(struct platform_device *pdev);


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

struct CONSYS_BASE_ADDRESS conn_reg;
static phys_addr_t gConEmiPhyBase;
static UINT8 __iomem *pEmibaseaddr;
static struct clk *clk_infra_conn_main;	/*ctrl infra_connmcu_bus clk */
static struct platform_device *my_pdev;
static struct reset_control *rstc;
static struct regulator *reg_VCN18;
static struct regulator *reg_VCN28;
static struct regulator *reg_VCN33_BT;
static struct regulator *reg_VCN33_WIFI;
static struct pinctrl *consys_pinctrl;
static struct pinctrl *mt6625_spi_pinctrl;
static struct pinctrl_state *mt6625_spi_default;
static struct regmap *pmic_regmap;
#define DYNAMIC_DUMP_GROUP_NUM 5

static const struct of_device_id apwmt_of_ids[] = {
	{.compatible = "mediatek,mt8163-consys",},
	{}
};
MODULE_DEVICE_TABLE(of, apwmt_of_ids);

static struct platform_driver mtk_wmt_dev_drv = {
	.probe = mtk_wmt_probe,
	.remove = mtk_wmt_remove,
	.driver = {
		   .name = "mt8163consys",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(apwmt_of_ids),
		   },
};

static INT32 mtk_wmt_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct reserved_mem *rmem;

	my_pdev = pdev;
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,pwrap-regmap", 0);
	if (node) {
		struct platform_device *pwrap_pdev = of_find_device_by_node(node);

		of_node_put(node);
		if (pwrap_pdev)
			pmic_regmap = dev_get_regmap(&pwrap_pdev->dev, NULL);
	}
	if (!pmic_regmap)
		WMT_PLAT_ERR_FUNC("no PMIC regmap: VCN28/VCN33 HW-mode bits stay as LK left them\n");

	reg_VCN18 = devm_regulator_get(&pdev->dev, "vcn18");
	reg_VCN28 = devm_regulator_get(&pdev->dev, "vcn28");
	reg_VCN33_BT = devm_regulator_get(&pdev->dev, "vcn33_bt");
	reg_VCN33_WIFI = devm_regulator_get(&pdev->dev, "vcn33_wifi");
	if (IS_ERR(reg_VCN18) || IS_ERR(reg_VCN28) || IS_ERR(reg_VCN33_BT) || IS_ERR(reg_VCN33_WIFI))
		return -EPROBE_DEFER;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = node ? of_reserved_mem_lookup(node) : NULL;
	of_node_put(node);
	if (!rmem || rmem->size < SZ_2M || (rmem->base & (SZ_2M - 1))) {
		WMT_PLAT_ERR_FUNC("consys needs a 2 MB aligned memory-region\n");
		return -EINVAL;
	}
	gConEmiPhyBase = rmem->base;
	/* stock mtk_wcn_consys_hw_init: CONN EMI remap = enable | MB offset of the region from the DRAM base */
	CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET,
			 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET) | 0x1000 |
			 ((gConEmiPhyBase >> 20) - 0x400));
	pEmibaseaddr = ioremap(gConEmiPhyBase + CONSYS_EMI_AP_PHY_OFFSET, CONSYS_EMI_MEM_SIZE);
	if (!pEmibaseaddr)
		return -ENOMEM;
	memset_io(pEmibaseaddr, 0, CONSYS_EMI_MEM_SIZE);
	WMT_PLAT_INFO_FUNC("EMI %pa, remap reg 0x%08x\n", &gConEmiPhyBase,
			   CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET));
	return 0;
}

static void mtk_wmt_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
}

/* stock spm_mtcmos_ctrl_connsys: SPM CONN_PWR_CON 0x280, PWR_STATUS(_2ND) bit 1, infracfg bus protect bits 15:13 */
#define SPM_CONN_PWR_CON	0x280
#define SPM_PWR_STATUS		0x60c
#define SPM_PWR_STATUS_2ND	0x610
#define INFRA_TOPAXI_PROTECTEN	0x1220	/* topckgen_base + 0x1000 = infracfg */
#define INFRA_TOPAXI_PROTECTSTA1 0x1228
#define CONN_PROT_MASK		0xe000

static void spm_rmw(UINT32 off, UINT32 clr, UINT32 set)
{
	CONSYS_REG_WRITE(conn_reg.spm_base + off, (CONSYS_REG_READ(conn_reg.spm_base + off) & ~clr) | set);
}

static INT32 spm_wait_conn(bool on)
{
	INT32 i;

	for (i = 0; i < 1000; i++) {
		UINT32 a = CONSYS_REG_READ(conn_reg.spm_base + SPM_PWR_STATUS) & BIT(1);
		UINT32 b = CONSYS_REG_READ(conn_reg.spm_base + SPM_PWR_STATUS_2ND) & BIT(1);

		if (on ? (a && b) : (!a && !b))
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static INT32 prot_wait(UINT32 want)
{
	INT32 i;

	for (i = 0; i < 1000; i++) {
		if ((CONSYS_REG_READ(conn_reg.topckgen_base + INFRA_TOPAXI_PROTECTSTA1) & CONN_PROT_MASK) == want)
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

VOID mtk_wcn_consys_power_on(VOID)
{
	SIZE_T prot = conn_reg.topckgen_base + INFRA_TOPAXI_PROTECTEN;

	spm_rmw(SPM_CONN_PWR_CON, 0, BIT(2));		/* PWR_ON */
	spm_rmw(SPM_CONN_PWR_CON, 0, BIT(3));		/* PWR_ON_2ND */
	if (spm_wait_conn(true))
		WMT_PLAT_ERR_FUNC("CONN MTCMOS: no power ack\n");
	spm_rmw(SPM_CONN_PWR_CON, BIT(4), 0);		/* ISO */
	spm_rmw(SPM_CONN_PWR_CON, BIT(1), 0);		/* CLK_DIS */
	spm_rmw(SPM_CONN_PWR_CON, 0, BIT(0));		/* RST_B */
	spm_rmw(SPM_CONN_PWR_CON, BIT(8), 0);		/* SRAM_PDN */
	CONSYS_REG_WRITE(prot, CONSYS_REG_READ(prot) & ~CONN_PROT_MASK);
	if (prot_wait(0))
		WMT_PLAT_ERR_FUNC("CONN bus protect release timed out\n");
	WMT_PLAT_INFO_FUNC("CONN MTCMOS on: PWR_CON 0x%08x\n", CONSYS_REG_READ(conn_reg.spm_base + SPM_CONN_PWR_CON));
}

VOID mtk_wcn_consys_power_off(VOID)
{
	SIZE_T prot = conn_reg.topckgen_base + INFRA_TOPAXI_PROTECTEN;

	CONSYS_REG_WRITE(prot, CONSYS_REG_READ(prot) | CONN_PROT_MASK);
	if (prot_wait(CONN_PROT_MASK))
		WMT_PLAT_ERR_FUNC("CONN bus protect set timed out\n");
	spm_rmw(SPM_CONN_PWR_CON, 0, BIT(8));		/* SRAM_PDN */
	spm_rmw(SPM_CONN_PWR_CON, 0, BIT(1));		/* CLK_DIS */
	spm_rmw(SPM_CONN_PWR_CON, BIT(0) | BIT(4), BIT(4));	/* RST_B low, ISO */
	spm_rmw(SPM_CONN_PWR_CON, BIT(2) | BIT(3), 0);	/* PWR_ON, PWR_ON_2ND */
	if (spm_wait_conn(false))
		WMT_PLAT_ERR_FUNC("CONN MTCMOS: still powered\n");
}

/* stock mtk_wcn_consys_hw_reg_ctrl (mt8163) */
#define CONSYS_RST_KEY		0x88000000
#define CONSYS_CPU_SW_RST_BIT	BIT(12)
#define CONSYS_TOP_CLKCG_BIT_8163 BIT(26)

INT32 mtk_wcn_consys_hw_reg_ctrl(UINT32 on, UINT32 co_clock_type)
{
	SIZE_T rst = conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET;
	UINT32 chip = 0, retry = 10;

	if (on) {
		if (pmic_regmap)
			regmap_update_bits(pmic_regmap, 0x512, BIT(1), 0);	/* VCN18 LP mode off */
		regulator_set_voltage(reg_VCN18, 1800000, 1800000);
		if (regulator_enable(reg_VCN18))
			WMT_PLAT_ERR_FUNC("enable VCN18 fail\n");
		udelay(150);
		if (pmic_regmap)	/* VCN28 HW control unless co-clock (then FM/GPS switch it) */
			regmap_update_bits(pmic_regmap, 0x41c, BIT(14), co_clock_type ? 0 : BIT(14));
		if (!co_clock_type) {
			regulator_set_voltage(reg_VCN28, 2800000, 2800000);
			if (regulator_enable(reg_VCN28))
				WMT_PLAT_ERR_FUNC("enable VCN28 fail\n");
		}
		CONSYS_REG_WRITE(rst, CONSYS_REG_READ(rst) | CONSYS_RST_KEY | CONSYS_CPU_SW_RST_BIT);
		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_TOP_CLKCG_CLR_OFFSET,
				 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_TOP_CLKCG_CLR_OFFSET) |
				 CONSYS_TOP_CLKCG_BIT_8163);
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_PWRON_CONFG_EN_OFFSET, CONSYS_PWRON_CONFG_EN_VALUE);
		mtk_wcn_consys_power_on();
		udelay(10);
		while (retry--) {
			chip = CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_CHIP_ID_OFFSET);
			if (chip == PLATFORM_SOC_CHIP)
				break;
			msleep(20);
		}
		WMT_PLAT_INFO_FUNC("CONSYS chip id 0x%08x\n", chip);
		CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_MCU_CFG_ACR_OFFSET,
				 CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_MCU_CFG_ACR_OFFSET) |
				 CONSYS_MCU_CFG_ACR_MBIST_BIT);
		CONSYS_REG_WRITE(rst, (CONSYS_REG_READ(rst) & ~(CONSYS_CPU_SW_RST_BIT | CONSYS_RST_KEY)) | CONSYS_RST_KEY);
		msleep(5);
	} else {
		mtk_wcn_consys_power_off();
		if (pmic_regmap)
			regmap_update_bits(pmic_regmap, 0x41c, BIT(14), 0);
		if (!co_clock_type && regulator_disable(reg_VCN28))
			WMT_PLAT_ERR_FUNC("disable VCN28 fail\n");
		if (regulator_disable(reg_VCN18))
			WMT_PLAT_ERR_FUNC("disable VCN18 fail\n");
	}
	return 0;
}

INT32 mtk_wcn_consys_hw_gpio_ctrl(UINT32 on)
{
	INT32 iRet = 0;

	WMT_PLAT_DBG_FUNC("CONSYS-HW-GPIO-CTRL(0x%08x), start\n", on);

	if (on) {

		/* TODO: [FixMe][GeorgeKuo] double check if BGF_INT is implemented ok */
		/* iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_MUX); */
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		WMT_PLAT_DBG_FUNC("CONSYS-HW, BGF IRQ registered and disabled\n");

	} else {

		/* set bgf eint/all eint to deinit state, namely input low state */
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT);
		WMT_PLAT_DBG_FUNC("CONSYS-HW, BGF IRQ unregistered and disabled\n");
		/* iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT); */
	}
	WMT_PLAT_DBG_FUNC("CONSYS-HW-GPIO-CTRL(0x%08x), finish\n", on);
	return iRet;

}

INT32 mtk_wcn_consys_hw_pwr_on(UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_INFO_FUNC("CONSYS-HW-PWR-ON, start\n");

	iRet += mtk_wcn_consys_hw_reg_ctrl(1, co_clock_type);
	iRet += mtk_wcn_consys_hw_gpio_ctrl(1);

	WMT_PLAT_INFO_FUNC("CONSYS-HW-PWR-ON, finish(%d)\n", iRet);
	return iRet;
}

INT32 mtk_wcn_consys_hw_pwr_off(VOID)
{
	INT32 iRet = 0;

	WMT_PLAT_INFO_FUNC("CONSYS-HW-PWR-OFF, start\n");

	iRet += mtk_wcn_consys_hw_reg_ctrl(0, 0);
	iRet += mtk_wcn_consys_hw_gpio_ctrl(0);

	WMT_PLAT_INFO_FUNC("CONSYS-HW-PWR-OFF, finish(%d)\n", iRet);
	return iRet;
}

INT32 mtk_wcn_consys_hw_rst(UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_INFO_FUNC("CONSYS-HW, hw_rst start, eirq should be disabled before this step\n");

	/*1. do whole hw power off flow */
	iRet += mtk_wcn_consys_hw_reg_ctrl(0, co_clock_type);

	/*2. do whole hw power on flow */
	iRet += mtk_wcn_consys_hw_reg_ctrl(1, co_clock_type);

	WMT_PLAT_INFO_FUNC("CONSYS-HW, hw_rst finish, eirq should be enabled after this step\n");
	return iRet;
}

#if CONSYS_BT_WIFI_SHARE_V33
INT32 mtk_wcn_consys_hw_bt_paldo_ctrl(UINT32 enable)
{
	/* spin_lock_irqsave(&gBtWifiV33.lock,gBtWifiV33.flags); */
	if (enable) {
		if (1 == gBtWifiV33.counter) {
			gBtWifiV33.counter++;
			WMT_PLAT_DBG_FUNC("V33 has been enabled,counter(%d)\n", gBtWifiV33.counter);
		} else if (2 == gBtWifiV33.counter) {
			WMT_PLAT_DBG_FUNC("V33 has been enabled,counter(%d)\n", gBtWifiV33.counter);
		} else {
#if CONSYS_PMIC_CTRL_ENABLE
			/*do BT PMIC on,depenency PMIC API ready */
			/*switch BT PALDO control from SW mode to HW mode:0x416[5]-->0x1 */
			/* VOL_DEFAULT, VOL_3300, VOL_3400, VOL_3500, VOL_3600 */
			hwPowerOn(MT6323_POWER_LDO_VCN33, VOL_3300, "wcn_drv");
			upmu_set_vcn33_on_ctrl_bt(1);
#endif
			WMT_PLAT_INFO_FUNC("WMT do BT/WIFI v3.3 on\n");
			gBtWifiV33.counter++;
		}

	} else {
		if (1 == gBtWifiV33.counter) {
			/*do BT PMIC off */
			/*switch BT PALDO control from HW mode to SW mode:0x416[5]-->0x0 */
#if CONSYS_PMIC_CTRL_ENABLE
		    upmu_set_vcn33_on_ctrl_bt(0);
			hwPowerDown(MT6323_POWER_LDO_VCN33, "wcn_drv");
#endif
			WMT_PLAT_INFO_FUNC("WMT do BT/WIFI v3.3 off\n");
			gBtWifiV33.counter--;
		} else if (2 == gBtWifiV33.counter) {
			gBtWifiV33.counter--;
			WMT_PLAT_DBG_FUNC("V33 no need disabled,counter(%d)\n", gBtWifiV33.counter);
		} else {
			WMT_PLAT_DBG_FUNC("V33 has been disabled,counter(%d)\n", gBtWifiV33.counter);
		}

	}
	/* spin_unlock_irqrestore(&gBtWifiV33.lock,gBtWifiV33.flags); */
	return 0;
}

INT32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT32 enable)
{
	mtk_wcn_consys_hw_bt_paldo_ctrl(enable);
	return 0;
}

#else
INT32 mtk_wcn_consys_hw_bt_paldo_ctrl(UINT32 enable)
{

	if (enable) {
		/*do BT PMIC on,depenency PMIC API ready */
		/*switch BT PALDO control from SW mode to HW mode:0x416[5]-->0x1 */
		if (reg_VCN33_BT) {
			regulator_set_voltage(reg_VCN33_BT, 3300000, 3300000);
			if (regulator_enable(reg_VCN33_BT))
				WMT_PLAT_ERR_FUNC("WMT do BT PMIC on fail!\n");
		}
		regmap_update_bits(pmic_regmap, 0x416, 0x1 << 5, 0x1 << 5);/*BT*/
		WMT_PLAT_INFO_FUNC("WMT do BT PMIC on\n");
	} else {
		/*do BT PMIC off */
		/*switch BT PALDO control from HW mode to SW mode:0x416[5]-->0x0 */
		regmap_update_bits(pmic_regmap, 0x416, 0x1 << 5, 0x0 << 5);/*BT*/
		if (reg_VCN33_BT)
			if (regulator_disable(reg_VCN33_BT))
				WMT_PLAT_ERR_FUNC("WMT do BT PMIC off fail!\n");
		WMT_PLAT_INFO_FUNC("WMT do BT PMIC off\n");
	}

	return 0;

}

INT32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT32 enable)
{

	if (enable) {
		/*do WIFI PMIC on,depenency PMIC API ready */
		/*switch WIFI PALDO control from SW mode to HW mode:0x418[14]-->0x1 */
		if (reg_VCN33_WIFI) {
			regulator_set_voltage(reg_VCN33_WIFI, 3300000, 3300000);
			if (regulator_enable(reg_VCN33_WIFI))
				WMT_PLAT_ERR_FUNC("WMT do WIFI PMIC on fail!\n");
			else
				WMT_PLAT_INFO_FUNC("WMT do WIFI PMIC on !\n");
		}
		regmap_update_bits(pmic_regmap, 0x418, 0x1 << 14, 0x1 << 14);/*WIFI*/
		WMT_PLAT_INFO_FUNC("WMT do WIFI PMIC on\n");
	} else {
		/*do WIFI PMIC off */
		/*switch WIFI PALDO control from HW mode to SW mode:0x418[14]-->0x0 */
		regmap_update_bits(pmic_regmap, 0x418, 0x1 << 14, 0x0 << 14);/*WIFI*/
		if (reg_VCN33_WIFI)
			if (regulator_disable(reg_VCN33_WIFI))
				WMT_PLAT_ERR_FUNC("WMT do WIFI PMIC off fail!\n");
		WMT_PLAT_INFO_FUNC("WMT do WIFI PMIC off\n");
	}

	return 0;
}

#endif
EXPORT_SYMBOL(mtk_wcn_consys_hw_wifi_paldo_ctrl);
INT32 mtk_wcn_consys_hw_vcn28_ctrl(UINT32 enable)
{
	if (enable) {
		/*in co-clock mode,need to turn on vcn28 when fm on */
		if (reg_VCN28) {
			regulator_set_voltage(reg_VCN28, 2800000, 2800000);
			if (regulator_enable(reg_VCN28))
				WMT_PLAT_ERR_FUNC("WMT do VCN28 PMIC on fail!\n");
		}
		WMT_PLAT_INFO_FUNC("turn on vcn28 for fm/gps usage in co-clock mode\n");
	} else {
		/*in co-clock mode,need to turn off vcn28 when fm off */
		if (reg_VCN28)
			if (regulator_disable(reg_VCN28))
				WMT_PLAT_ERR_FUNC("WMT do VCN28 PMIC off fail!\n");
		WMT_PLAT_INFO_FUNC("turn off vcn28 for fm/gps usage in co-clock mode\n");
	}
	return 0;
}

INT32 mtk_wcn_consys_hw_state_show(VOID)
{
	return 0;
}

INT32 mtk_wcn_consys_hw_restore(struct device *device)
{
	UINT32 addrPhy = 0;

	gConEmiPhyBase = CONSYS_EMI_AP_PHY_BASE - CONSYS_EMI_AP_PHY_OFFSET;
	if (gConEmiPhyBase) {

#if CONSYS_EMI_MPU_SETTING
		/*set MPU for EMI share Memory */
		WMT_PLAT_INFO_FUNC("setting MPU for EMI share memory\n");

#if 0
	emi_mpu_set_region_protection(gConEmiPhyBase + SZ_1M/2,
		gConEmiPhyBase + SZ_1M,
		5,
		SET_ACCESS_PERMISSON(FORBIDDEN, NO_PROTECTION, FORBIDDEN, NO_PROTECTION));


#else
		WMT_PLAT_WARN_FUNC("not define platform config\n");
#endif

#endif
		(void)addrPhy; /* no CONSYS EMI remap register on mt6572 (spike 2026-07-14);
		 * this whole restore cb is dead on mainline anyway (CONFIG_MTK_HIBERNATION) */

#if 1
		pEmibaseaddr = ioremap(gConEmiPhyBase + CONSYS_EMI_AP_PHY_OFFSET, CONSYS_EMI_MEM_SIZE);
#else
		pEmibaseaddr = ioremap(CONSYS_EMI_AP_PHY_BASE, CONSYS_EMI_MEM_SIZE);
#endif
		if (pEmibaseaddr) {
			WMT_PLAT_INFO_FUNC("EMI mapping OK(0x%p)\n", pEmibaseaddr);
			memset_io(pEmibaseaddr, 0, CONSYS_EMI_MEM_SIZE);
		} else {
			WMT_PLAT_ERR_FUNC("EMI mapping fail\n");
		}
	} else {
		WMT_PLAT_ERR_FUNC("consys emi memory address gConEmiPhyBase invalid\n");
	}

	return 0;
}

/*Reserved memory by device tree!*/
/* mt6572: EMI diagnostics window is at a fixed AP-phys address that the
 * FW-side base (0xf0080000) falls through to; there is no remap register
 * on this chip and the region lies outside kernel-managed RAM. */


INT32 mtk_wcn_consys_hw_init(void)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-consys");
	INT32 ret;

	if (!node) {
		WMT_PLAT_ERR_FUNC("no mediatek,mt8163-consys node\n");
		return -ENODEV;
	}
	conn_reg.mcu_base = (SIZE_T)of_iomap(node, 0);
	conn_reg.ap_rgu_base = (SIZE_T)of_iomap(node, 1);
	conn_reg.topckgen_base = (SIZE_T)of_iomap(node, 2);
	conn_reg.spm_base = (SIZE_T)of_iomap(node, 3);
	of_node_put(node);
	if (!conn_reg.mcu_base || !conn_reg.ap_rgu_base || !conn_reg.topckgen_base || !conn_reg.spm_base)
		return -ENOMEM;
	ret = platform_driver_register(&mtk_wmt_dev_drv);
	if (ret)
		WMT_PLAT_ERR_FUNC("WMT platform driver registration failed(%d)\n", ret);
	return ret;
}

INT32 mtk_wcn_consys_hw_deinit(void)
{
	if (pEmibaseaddr) {
		iounmap(pEmibaseaddr);
		pEmibaseaddr = NULL;
	}
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_CONNSYS);
#endif

	platform_driver_unregister(&mtk_wmt_dev_drv);
	return 0;
}

UINT8 *mtk_wcn_consys_emi_virt_addr_get(UINT32 ctrl_state_offset)
{
	UINT8 *p_virtual_addr = NULL;

	if (!pEmibaseaddr) {
		WMT_PLAT_ERR_FUNC("EMI base address is NULL\n");
		return NULL;
	}
	WMT_PLAT_DBG_FUNC("ctrl_state_offset(%08x)\n", ctrl_state_offset);
	p_virtual_addr = pEmibaseaddr + ctrl_state_offset;

	return p_virtual_addr;
}

UINT32 mtk_wcn_consys_soc_chipid(void)
{
	return PLATFORM_SOC_CHIP;
}

struct pinctrl *mtk_wcn_consys_get_pinctrl()
{
	return consys_pinctrl;
}
INT32 mtk_wcn_consys_set_dynamic_dump(PUINT32 str_buf)
{
	PUINT8 vir_addr = NULL;

	vir_addr = mtk_wcn_consys_emi_virt_addr_get(EXP_APMEM_CTRL_CHIP_DYNAMIC_DUMP);
	if (!vir_addr) {
		WMT_PLAT_ERR_FUNC("get vir address fail\n");
		return -2;
	}
	memcpy(vir_addr, str_buf, DYNAMIC_DUMP_GROUP_NUM*8);
	WMT_PLAT_INFO_FUNC("dynamic dump register value(0x%08x)\n", CONSYS_REG_READ(vir_addr));
	return 0;
}
