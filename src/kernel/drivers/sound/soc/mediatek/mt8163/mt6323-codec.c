// SPDX-License-Identifier: GPL-2.0
/*
 * MT6323 audio codec (DAC + class-D speaker amp + headphone amp, ADC1/ADC2 for the microphone) as the Teclast
 * P80h stock kernel drives it. Every sequence is the register trace of the stock function named next to it,
 * recovered with `p80h re audio` (emulator over the stock kernel, src/re/golden/audio-*.txt), at 48 kHz.
 * The PMIC is reached through the pwrap regmap, as in mt6323-reboot-mode.c. The digital side (0x40xx) latches a
 * new configuration on a toggle handshake: bit 8/9 of 0x4016 is flipped against status bit 0/1
 * (stock TopCtlChangeTrigger and its DL twin).
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#define TOP_CKPDN_SET	0x010a
#define TOP_CKPDN_CLR	0x010c
#define AUDTOP_CON0	0x0700	/* bits 6:4 mic PGA L */
#define AUDTOP_CON1	0x0702	/* bits 10:8 mic PGA R */
#define AUDTOP_CON5	0x070a	/* bits 14:12 / 10:8 headphone PGA L / R, 0 = loudest, 7 = ramp start */
#define SPK_CON9	0x0064	/* bits 11:8 speaker PGA: 0 mute, 1 = 0 dB, 2..15 = +4..+17 dB */
#define AFE_UL_DL_CON0	0x4000
#define AFE_TRIG	0x4016

/* 0 delay = none; TRIG_UL/TRIG_DL: run the handshake instead of a write */
enum { WR, TRIG_UL, TRIG_DL };
struct seq { u8 op; u16 reg, val, mask; u16 delay_us; };
#define W(r, v, m, ...) { WR, r, v, m, ##__VA_ARGS__ }

static const struct seq dac_on[] = {	/* TurnOnDacPower: NEWIF_CFG0 = 0x330 | GetDLNewIFFrequency(48000) = 8 << 12 */
	W(0x4024, 0x8330, 0xffff), W(0x4002, 0x000a, 0x000f), W(AFE_UL_DL_CON0, 0x0001, 0x0001), { TRIG_UL },
};
static const struct seq dac_off[] = {	/* TurnOffDacPower tail */
	W(0x0708, 0x0000, 0xffff), W(0x0700, 0x0000, 0x1000), W(0x070c, 0x37e2, 0xffff),
	W(AFE_UL_DL_CON0, 0x0000, 0x0001), { TRIG_UL },
};
static const struct seq spk_on[] = {	/* Speaker_Amp_Change(1) after TurnOnDacPower */
	W(0x070e, 0x2400, 0xffff), W(0x070c, 0xb7f6, 0xffff), W(0x0700, 0x7000, 0xf000), W(0x0708, 0x0014, 0xffff, 10000),
	W(0x070e, 0x35b0, 0xffff), W(TOP_CKPDN_CLR, 0x000e, 0x000e),
	W(0x0056, 0x0214, 0xffff), W(SPK_CON9, 0x0400, 0xffff),	/* Speaker_ClassD_Open */
	W(0x0052, 0x3008, 0xffff), W(0x0052, 0x3009, 0xffff, 5000), W(0x0052, 0x3001, 0xffff),
	W(0x006a, 0x0a00, 0xffff, 1000),
	W(0x070e, 0x3506, 0xffff, 1000), W(0x070e, 0x3507, 0xffff, 1000), W(0x070e, 0x3508, 0xffff, 1000),
	W(0x070e, 0x3509, 0xffff, 1000), W(0x070e, 0x350a, 0xffff, 1000), W(0x070e, 0x350b, 0xffff),
};
static const struct seq spk_off[] = {	/* Speaker_Amp_Change(0), before TurnOffDacPower */
	W(0x070e, 0x350a, 0xffff, 1000), W(0x070e, 0x3509, 0xffff, 1000), W(0x070e, 0x3508, 0xffff, 1000),
	W(0x070e, 0x3507, 0xffff, 1000), W(0x070e, 0x3506, 0xffff, 1000), W(0x070e, 0x3505, 0xffff, 1000),
	W(0x0052, 0x0004, 0xffff), W(0x006a, 0x0000, 0xffff), W(TOP_CKPDN_SET, 0x000e, 0x000e),
	W(0x070e, 0x2500, 0xffff), W(0x070e, 0x2400, 0xffff),
};
static const struct seq hp_on[] = {	/* Audio_Amp_Change(1) after TurnOnDacPower */
	W(0x4006, 0x0000, 0xffff), W(0x4008, 0x0000, 0xffff), W(0x4014, 0x0001, 0x0001), { TRIG_DL },
	W(0x070c, 0xf7f2, 0xffff), W(0x0700, 0x7000, 0xf000), W(AUDTOP_CON5, 0x0014, 0xffff), W(0x0708, 0x007c, 0xffff),
	W(0x070c, 0xf5ba, 0xffff), W(AUDTOP_CON5, 0x2214, 0xffff), W(AUDTOP_CON5, 0x7700, 0x7700),
};
static const struct seq hp_off[] = {	/* Audio_Amp_Change(0), before TurnOffDacPower */
	W(AUDTOP_CON5, 0x0014, 0xffff), W(0x070c, 0xf7f2, 0xffff),
};
static const struct seq adc_on[] = {	/* TurnOnADcPowerACC(ADC1 = 13, 1) then (ADC2 = 14, 1), mic source 0 */
	W(0x4028, 0x3c2f, 0xffff),	/* 0x302f | GetULNewIFFrequency(48000) = 3 << 10 */
	W(0x4002, 0x0010, 0x0010), { TRIG_UL }, W(AFE_UL_DL_CON0, 0x0002, 0x0002),
	W(0x0700, 0x7800, 0x7f80), W(0x0704, 0x00f0, 0x00ff), W(0x0706, 0x0000, 0x0100), W(0x0700, 0x0000, 0x000f),
	W(0x0710, 0x0008, 0x0008), W(0x0700, 0x0180, 0x0180), W(0x0704, 0x0003, 0x0003),
	W(0x0706, 0x0200, 0x0200), W(AUDTOP_CON1, 0x0040, 0x00f0),
};
static const struct seq adc_off[] = {	/* TurnOnADcPowerACC(14, 0) then (13, 0) */
	W(0x0700, 0x0000, 0x0180), W(0x0704, 0x0000, 0x0003), W(0x0700, 0x7000, 0x7f80), W(0x0704, 0x00c0, 0x00ff),
	W(0x0706, 0x0000, 0x0100), W(0x0700, 0x0003, 0x000f), W(0x0706, 0x0000, 0x0200), W(AUDTOP_CON1, 0x0020, 0x00f0),
	W(0x0710, 0x0000, 0x0008), W(AFE_UL_DL_CON0, 0x0000, 0x0002),
};

struct mt6323_codec {
	struct regmap *map;
	struct mutex lock;
	int topck;	/* users of the audio top clock (Topck_Enable refcount) */
	unsigned int spk_gain, hp_gain[2], mic_gain[2];
};

static void trigger(struct regmap *map, unsigned int status, unsigned int toggle)
{
	unsigned int v = 0;

	regmap_read(map, AFE_TRIG, &v);
	regmap_update_bits(map, AFE_TRIG, toggle, (v & status) ? 0 : toggle);
}

static void run(struct regmap *map, const struct seq *s, int n)
{
	for (; n--; s++) {
		if (s->op == TRIG_UL)
			trigger(map, BIT(0), BIT(8));
		else if (s->op == TRIG_DL)
			trigger(map, BIT(1), BIT(9));
		else
			regmap_update_bits(map, s->reg, s->mask, s->val);
		if (s->delay_us)
			fsleep(s->delay_us);
	}
}
#define RUN(c, s) run((c)->map, s, ARRAY_SIZE(s))

static void topck(struct mt6323_codec *c, bool on)	/* Topck_Enable: CKPDN bit 8 (audio top clock) */
{
	if (on && c->topck++ == 0)
		regmap_update_bits(c->map, TOP_CKPDN_CLR, 0x0100, 0x0100);
	else if (!on && --c->topck == 0)
		regmap_update_bits(c->map, TOP_CKPDN_SET, 0x0100, 0x0100);
}

static struct mt6323_codec *codec_of(struct snd_soc_dapm_widget *w)
{
	return snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
}

static int dac_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
	struct mt6323_codec *c = codec_of(w);

	mutex_lock(&c->lock);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		regmap_update_bits(c->map, 0x0102, 0x0001, 0x0001);	/* AudDrv_ANA_Clk_On: RG_CLKSQ_EN_AUD */
		topck(c, true);
		RUN(c, dac_on);
	} else {
		RUN(c, dac_off);
		topck(c, false);
	}
	mutex_unlock(&c->lock);
	return 0;
}

static int spk_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
	struct mt6323_codec *c = codec_of(w);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		RUN(c, spk_on);
		regmap_update_bits(c->map, SPK_CON9, 0x0f00, c->spk_gain << 8);
	} else {
		RUN(c, spk_off);
	}
	return 0;
}

static int hp_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
	struct mt6323_codec *c = codec_of(w);
	int step;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		RUN(c, hp_on);
		for (step = 7; step >= 0; step--) {	/* ramp down from the quietest step: no pop */
			unsigned int l = max_t(int, step, 7 - c->hp_gain[0]), r = max_t(int, step, 7 - c->hp_gain[1]);

			regmap_update_bits(c->map, AUDTOP_CON5, 0x7700, l << 12 | r << 8);
			usleep_range(1000, 1500);
		}
	} else {
		regmap_update_bits(c->map, AUDTOP_CON5, 0x7700, 0x7700);
		RUN(c, hp_off);
	}
	return 0;
}

static int adc_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
	struct mt6323_codec *c = codec_of(w);

	mutex_lock(&c->lock);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		regmap_update_bits(c->map, 0x0102, 0x0001, 0x0001);
		topck(c, true);
		RUN(c, adc_on);
		regmap_update_bits(c->map, AUDTOP_CON0, 0x0070, c->mic_gain[0] << 4);	/* SetMicPGAGain */
		regmap_update_bits(c->map, AUDTOP_CON1, 0x0700, c->mic_gain[1] << 8);
	} else {
		RUN(c, adc_off);
		topck(c, false);
	}
	mutex_unlock(&c->lock);
	return 0;
}

/* volume controls are cached and applied live (the PGAs only matter while their path is powered) */
static int gain_get(struct snd_kcontrol *k, struct snd_ctl_elem_value *v)
{
	struct mt6323_codec *c = snd_soc_component_get_drvdata(snd_kcontrol_chip(k));
	unsigned int *g = (unsigned int *)((char *)c + k->private_value);

	v->value.integer.value[0] = g[0];
	if (g != &c->spk_gain)
		v->value.integer.value[1] = g[1];
	return 0;
}

static int gain_put(struct snd_kcontrol *k, struct snd_ctl_elem_value *v)
{
	struct mt6323_codec *c = snd_soc_component_get_drvdata(snd_kcontrol_chip(k));
	unsigned int *g = (unsigned int *)((char *)c + k->private_value), max, n, i, val, changed = 0;

	max = g == &c->spk_gain ? 15 : g == c->hp_gain ? 7 : 5;
	n = g == &c->spk_gain ? 1 : 2;
	for (i = 0; i < n; i++) {
		val = clamp_t(long, v->value.integer.value[i], 0, max);
		changed |= val != g[i];
		g[i] = val;
	}
	if (g == &c->spk_gain) {
		regmap_update_bits(c->map, SPK_CON9, 0x0f00, g[0] << 8);
	} else if (g == c->hp_gain) {
		unsigned int con5 = 0;

		regmap_read(c->map, AUDTOP_CON5, &con5);
		if ((con5 & 0x7700) != 0x7700)	/* amp is on (not parked at the ramp start) */
			regmap_update_bits(c->map, AUDTOP_CON5, 0x7700, (7 - g[0]) << 12 | (7 - g[1]) << 8);
	} else {
		regmap_update_bits(c->map, AUDTOP_CON0, 0x0070, g[0] << 4);
		regmap_update_bits(c->map, AUDTOP_CON1, 0x0700, g[1] << 8);
	}
	return changed;
}

static int gain_info(struct snd_kcontrol *k, struct snd_ctl_elem_info *u)
{
	struct mt6323_codec *c = snd_soc_component_get_drvdata(snd_kcontrol_chip(k));
	unsigned int *g = (unsigned int *)((char *)c + k->private_value);

	u->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	u->count = g == &c->spk_gain ? 1 : 2;
	u->value.integer.min = 0;
	u->value.integer.max = g == &c->spk_gain ? 15 : g == c->hp_gain ? 7 : 5;
	return 0;
}

#define GAIN(xname, field, xtlv) { .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ, .tlv.p = xtlv, \
	.info = gain_info, .get = gain_get, .put = gain_put, .private_value = offsetof(struct mt6323_codec, field) }

/* speaker: step 0 mute, then 0 dB and 4..17 dB (vendor speaker_PGA_function); mic preamp 0..30 dB in 6 dB */
static const DECLARE_TLV_DB_RANGE(spk_tlv, 0, 0, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
				  1, 1, TLV_DB_SCALE_ITEM(0, 0, 0), 2, 15, TLV_DB_SCALE_ITEM(400, 100, 0));
static const DECLARE_TLV_DB_SCALE(hp_tlv, -700, 100, 0);
static const DECLARE_TLV_DB_SCALE(mic_tlv, 0, 600, 0);

static const struct snd_kcontrol_new controls[] = {
	GAIN("Speaker Playback Volume", spk_gain, spk_tlv),
	GAIN("Headphone Playback Volume", hp_gain, hp_tlv),
	GAIN("Mic Capture Volume", mic_gain, mic_tlv),
};

/* the P80h speaker hangs off the HP outputs through an external amp (stock: AUDTOP_CON4/6 = HP amp, SPK_CON0 off),
 * so the HP amp is the default; "Speaker" here is the MT6323's own class-D, unused on this board */
static const char * const out_texts[] = { "Headphone", "Speaker" };
static SOC_ENUM_SINGLE_VIRT_DECL(out_enum, out_texts);
static const struct snd_kcontrol_new out_mux = SOC_DAPM_ENUM("Playback Route", out_enum);

static const struct snd_soc_dapm_widget widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0, dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* demux: only the selected amp powers up (both share AUDTOP 0x700/0x708/0x70c settings) */
	SND_SOC_DAPM_DEMUX("Output", SND_SOC_NOPM, 0, 0, &out_mux),
	SND_SOC_DAPM_OUT_DRV_E("Speaker Amp", SND_SOC_NOPM, 0, 0, NULL, 0, spk_event,
			       SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("Headphone Amp", SND_SOC_NOPM, 0, 0, NULL, 0, hp_event,
			       SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("SPK"),
	SND_SOC_DAPM_OUTPUT("HP"),
	SND_SOC_DAPM_ADC_E("ADC", "Capture", SND_SOC_NOPM, 0, 0, adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_INPUT("MIC"),
};

static const struct snd_soc_dapm_route routes[] = {
	{ "Output", NULL, "DAC" },
	{ "Speaker Amp", "Speaker", "Output" },
	{ "Headphone Amp", "Headphone", "Output" },
	{ "SPK", NULL, "Speaker Amp" },
	{ "HP", NULL, "Headphone Amp" },
	{ "ADC", NULL, "MIC" },
};

static const struct snd_soc_component_driver component = {
	.controls = controls, .num_controls = ARRAY_SIZE(controls),
	.dapm_widgets = widgets, .num_dapm_widgets = ARRAY_SIZE(widgets),
	.dapm_routes = routes, .num_dapm_routes = ARRAY_SIZE(routes),
	.idle_bias_on = false,
	.endianness = 1,
};

static struct snd_soc_dai_driver dai = {
	.name = "mt6323-aif",
	.playback = {
		.stream_name = "Playback", .channels_min = 2, .channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000, .formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "Capture", .channels_min = 2, .channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000, .formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static int mt6323_codec_probe(struct platform_device *pdev)
{
	struct mt6323_codec *c;
	struct device_node *np;
	struct platform_device *pmic;

	c = devm_kzalloc(&pdev->dev, sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;
	np = of_parse_phandle(pdev->dev.of_node, "mediatek,pmic", 0);
	if (!np)
		return -EINVAL;
	pmic = of_find_device_by_node(np);
	of_node_put(np);
	if (!pmic || !pmic->dev.parent)
		return -EPROBE_DEFER;
	c->map = dev_get_regmap(pmic->dev.parent, NULL);
	put_device(&pmic->dev);
	if (!c->map)
		return -EPROBE_DEFER;
	mutex_init(&c->lock);
	c->spk_gain = 1;		/* 0 dB, the stock default (Speaker_pga_gain = 1) */
	c->hp_gain[0] = c->hp_gain[1] = 4;
	c->mic_gain[0] = c->mic_gain[1] = 3;	/* 18 dB */
	platform_set_drvdata(pdev, c);
	return devm_snd_soc_register_component(&pdev->dev, &component, &dai, 1);
}

static const struct of_device_id mt6323_codec_of_match[] = {
	{ .compatible = "mediatek,mt6323-codec" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6323_codec_of_match);

static struct platform_driver mt6323_codec_driver = {
	.probe = mt6323_codec_probe,
	.driver = { .name = "mt6323-codec", .of_match_table = mt6323_codec_of_match },
};
module_platform_driver(mt6323_codec_driver);
MODULE_LICENSE("GPL");
