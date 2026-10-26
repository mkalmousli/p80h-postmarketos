// SPDX-License-Identifier: GPL-2.0
/*
 * Battery voltage and state of charge from the MT6323 PMIC AUXADC, as the Teclast P80h stock kernel reads it
 * (`p80h re`, PMIC_IMM_GetOneChannelValue(7 = BATSNS)): RG_VBUF_EN (AUXADC_CON11 0x758 bit 4) on, toggle the
 * channel's bit in RG_AP_RQST_LIST (AUXADC_CON22 0x76e bits 8:0), wait RDY (bit 15) of AUXADC_ADC0 (0x714),
 * value = bits 14:0 * 4 * 1800 mV / 32768. Capacity from the stock OCV profile (simple-battery table in the DT,
 * `p80h re battery`); charge state from the charger (FAN5405, bq2415x driver). No coulomb counter: the stock
 * gauge's ISENSE path is not used, so the estimate is filtered and kept monotonic per charge/discharge phase.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>

#define AUXADC_ADC0	0x0714	/* BATSNS */
#define ADC_RDY		BIT(15)
#define ADC_OUT		GENMASK(14, 0)
#define AUXADC_CON11	0x0758
#define VBUF_EN		BIT(4)
#define AUXADC_CON22	0x076e
#define CH_BATSNS	BIT(7)
#define SAMPLES		8
#define POLL		(30 * HZ)

struct mt6323_bat {
	struct regmap *map;
	struct power_supply *psy, *charger;
	struct power_supply_battery_info *info;
	struct delayed_work work;
	struct mutex lock;
	int uv;		/* filtered battery voltage */
	int cap;	/* reported capacity, -1 until the first sample */
	int status;
};

static int read_batsns_uv(struct mt6323_bat *b)
{
	unsigned int v = 0, sum = 0;
	int i, t;

	regmap_update_bits(b->map, AUXADC_CON11, VBUF_EN, VBUF_EN);
	for (i = 0; i < SAMPLES; i++) {
		regmap_update_bits(b->map, AUXADC_CON22, CH_BATSNS, 0);
		regmap_update_bits(b->map, AUXADC_CON22, CH_BATSNS, CH_BATSNS);
		udelay(300);	/* stock: "HW limitation" settle before polling */
		for (t = 0; t < 100; t++) {
			if (regmap_read(b->map, AUXADC_ADC0, &v))
				return -EIO;
			if (v & ADC_RDY)
				break;
			usleep_range(1000, 2000);
		}
		if (!(v & ADC_RDY))
			return -ETIMEDOUT;
		sum += v & ADC_OUT;
	}
	return (int)div_u64((u64)sum * 4 * 1800 * 1000, SAMPLES * 32768);
}

static int charger_status(struct mt6323_bat *b)
{
	union power_supply_propval val;

	if (!b->charger)
		b->charger = power_supply_get_by_phandle(b->psy->dev.parent->of_node, "power-supplies");
	if (IS_ERR_OR_NULL(b->charger)) {
		b->charger = NULL;
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}
	if (power_supply_get_property(b->charger, POWER_SUPPLY_PROP_STATUS, &val))
		return POWER_SUPPLY_STATUS_UNKNOWN;
	return val.intval;
}

static void update(struct mt6323_bat *b)
{
	int uv = read_batsns_uv(b), status = charger_status(b), cap;

	if (uv < 0)
		return;
	mutex_lock(&b->lock);
	b->uv = b->cap < 0 ? uv : (3 * b->uv + uv) / 4;
	cap = power_supply_batinfo_ocv2cap(b->info, b->uv, 25);
	if (status == POWER_SUPPLY_STATUS_FULL)
		cap = 100;
	if (b->cap >= 0 && status == b->status) {
		/* without a coulomb counter IR drop moves the estimate both ways: hold it monotonic in a phase */
		if (status == POWER_SUPPLY_STATUS_CHARGING)
			cap = max(cap, b->cap);
		else if (status == POWER_SUPPLY_STATUS_DISCHARGING || status == POWER_SUPPLY_STATUS_NOT_CHARGING)
			cap = min(cap, b->cap);
	}
	b->cap = clamp(cap, 0, 100);
	b->status = status;
	mutex_unlock(&b->lock);
}

static void work_fn(struct work_struct *w)
{
	struct mt6323_bat *b = container_of(to_delayed_work(w), struct mt6323_bat, work);
	int old = b->cap, old_status = b->status;

	update(b);
	if (b->cap != old || b->status != old_status)
		power_supply_changed(b->psy);
	schedule_delayed_work(&b->work, POLL);
}

static void external_power_changed(struct power_supply *psy)
{
	struct mt6323_bat *b = power_supply_get_drvdata(psy);

	mod_delayed_work(system_wq, &b->work, HZ);	/* charger state settles first */
}

static const enum power_supply_property props[] = {
	POWER_SUPPLY_PROP_STATUS, POWER_SUPPLY_PROP_PRESENT, POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW, POWER_SUPPLY_PROP_CAPACITY, POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, POWER_SUPPLY_PROP_CHARGE_FULL, POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
};

static int get_property(struct power_supply *psy, enum power_supply_property p, union power_supply_propval *v)
{
	struct mt6323_bat *b = power_supply_get_drvdata(psy);

	if (b->cap < 0 && p != POWER_SUPPLY_PROP_PRESENT && p != POWER_SUPPLY_PROP_TECHNOLOGY)
		return -ENODATA;
	switch (p) {
	case POWER_SUPPLY_PROP_STATUS:
		v->intval = b->status == POWER_SUPPLY_STATUS_UNKNOWN ? POWER_SUPPLY_STATUS_DISCHARGING : b->status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		v->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		v->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		v->intval = b->uv;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		v->intval = b->cap;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		v->intval = b->cap >= 100 ? POWER_SUPPLY_CAPACITY_LEVEL_FULL :
			    b->cap > 15 ? POWER_SUPPLY_CAPACITY_LEVEL_NORMAL :
			    b->cap > 5 ? POWER_SUPPLY_CAPACITY_LEVEL_LOW : POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		v->intval = b->info->charge_full_design_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:	/* tint2 derives its percentage from charge_now/charge_full */
		v->intval = b->info->charge_full_design_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		v->intval = (int)((s64)b->info->charge_full_design_uah * b->cap / 100);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		v->intval = b->info->voltage_max_design_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		v->intval = b->info->voltage_min_design_uv;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		v->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct power_supply_desc desc = {
	.name = "battery", .type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = props, .num_properties = ARRAY_SIZE(props),
	.get_property = get_property, .external_power_changed = external_power_changed,
};

static int mt6323_bat_probe(struct platform_device *pdev)
{
	struct power_supply_config cfg = { .of_node = pdev->dev.of_node };
	struct device_node *np;
	struct platform_device *pmic;
	struct mt6323_bat *b;
	int ret;

	b = devm_kzalloc(&pdev->dev, sizeof(*b), GFP_KERNEL);
	if (!b)
		return -ENOMEM;
	np = of_parse_phandle(pdev->dev.of_node, "mediatek,pmic", 0);
	if (!np)
		return -EINVAL;
	pmic = of_find_device_by_node(np);
	of_node_put(np);
	if (!pmic || !pmic->dev.parent)
		return -EPROBE_DEFER;
	b->map = dev_get_regmap(pmic->dev.parent, NULL);
	put_device(&pmic->dev);
	if (!b->map)
		return -EPROBE_DEFER;
	b->cap = -1;
	b->status = POWER_SUPPLY_STATUS_UNKNOWN;
	mutex_init(&b->lock);
	INIT_DELAYED_WORK(&b->work, work_fn);
	cfg.drv_data = b;
	platform_set_drvdata(pdev, b);
	b->psy = devm_power_supply_register(&pdev->dev, &desc, &cfg);
	if (IS_ERR(b->psy))
		return PTR_ERR(b->psy);
	ret = power_supply_get_battery_info(b->psy, &b->info);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "monitored-battery (simple-battery OCV table) missing\n");
	update(b);
	dev_info(&pdev->dev, "VBAT %d mV, %d%%\n", b->uv / 1000, b->cap);
	schedule_delayed_work(&b->work, POLL);
	return 0;
}

static void mt6323_bat_remove(struct platform_device *pdev)
{
	struct mt6323_bat *b = platform_get_drvdata(pdev);

	if (b)
		cancel_delayed_work_sync(&b->work);
}

static const struct of_device_id mt6323_bat_of_match[] = {
	{ .compatible = "mediatek,mt6323-battery" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6323_bat_of_match);

static struct platform_driver mt6323_bat_driver = {
	.probe = mt6323_bat_probe,
	.remove = mt6323_bat_remove,
	.driver = { .name = "mt6323-battery", .of_match_table = mt6323_bat_of_match },
};
module_platform_driver(mt6323_bat_driver);
MODULE_LICENSE("GPL");
