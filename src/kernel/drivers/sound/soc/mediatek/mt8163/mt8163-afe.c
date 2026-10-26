// SPDX-License-Identifier: GPL-2.0
/*
 * MT8163 audio front end: DL1 memif -> I05/I06 -> O03/O04 -> ADDA DL (to the MT6323 DAC) for playback, ADDA UL
 * (MT6323 ADC) -> I03/I04 -> O09/O10 -> VUL memif for capture. 48 kHz S16_LE stereo only. Register writes are the
 * stock kernel's (`p80h re audio`: mt_soc_pcm_dl1 / mt_soc_pcm_capture open/prepare/trigger, AudDrv_Clk_On,
 * AudDrv_IRQ_handler); the layout is the mt8173 one (DL1 0x40, VUL 0x80, IRQ_MCU 0x3a0..). Buffers in DRAM
 * (LK leaves M4U off, so the physical address is the bus address).
 */
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#define AUDIO_TOP_CON0	0x000	/* power-down bits: AFE 2, ADC 24, DAC 25, DAC_PREDIS 26; APB access 14, 31 */
#define TOP_APB_BITS	(BIT(31) | BIT(14))	/* stock runs with both set; without them AFE_CONNx ignore writes */
#define AFE_DAC_CON0	0x010	/* AFE_ON 0, memif n on = bit n + 1 */
#define AFE_DAC_CON1	0x014	/* DL1 rate 3:0, I2S rate 11:8, VUL rate 19:16, DL1 mono 21, VUL mono 27 */
#define AFE_CONN1	0x024
#define AFE_CONN2	0x028
#define AFE_CONN3	0x02c
#define AFE_I2S_CON1	0x034
#define AFE_DL1_BASE	0x040
#define AFE_DL1_CUR	0x044
#define AFE_DL1_END	0x048
#define AFE_CONN_24BIT	0x06c
#define AFE_VUL_BASE	0x080
#define AFE_VUL_END	0x088
#define AFE_VUL_CUR	0x08c
#define ADDA_DL_SRC2_CON0 0x108
#define ADDA_DL_SRC2_CON1 0x10c
#define ADDA_UL_SRC_CON0 0x114
#define ADDA_TOP_CON0	0x120
#define ADDA_UL_DL_CON0	0x124
#define ADDA_NEWIF_CFG0	0x138
#define ADDA_NEWIF_CFG1	0x13c
#define ADDA_PREDIS_CON0 0x260
#define ADDA_PREDIS_CON1 0x264
#define IRQ_MCU_CON	0x3a0	/* IRQ1 (DL1) en 0 rate 7:4, IRQ2 (VUL) en 1 rate 11:8 */
#define IRQ_MCU_STATUS	0x3a4
#define IRQ_MCU_CLR	0x3a8
#define IRQ_MCU_CNT1	0x3ac
#define IRQ_MCU_CNT2	0x3b0
#define MEMIF_HD_CON	0x3d8	/* DL1 fetch format 17:16, VUL 23:22 (0 = 16 bit) */
#define AFE_4C4		0x4c4
#define RATE_48K	0xa
#define SPM_AUDIO_PWR_CON 0x29c
#define SPM_PWR_STATUS	0x60c
#define SPM_PWR_STATUS2	0x610
#define PWR_STATUS_AUDIO BIT(24)

struct mt8163_afe {
	void __iomem *base, *spm;
	struct clk_bulk_data clks[3];
	struct snd_pcm_substream *sub[2];
	struct mutex lock;
	int users;	/* open streams: clocks, AFE_ON, ADDA */
};

static void upd(struct mt8163_afe *afe, u32 reg, u32 mask, u32 val)
{
	writel((readl(afe->base + reg) & ~mask) | (val & mask), afe->base + reg);
}

/* stock spm_mtcmos_ctrl_aud(1); only needed when LK left the domain off */
static int audio_power_on(struct mt8163_afe *afe)
{
	void __iomem *con = afe->spm + SPM_AUDIO_PWR_CON;
	u32 v;

	if (readl(afe->spm + SPM_PWR_STATUS) & readl(afe->spm + SPM_PWR_STATUS2) & PWR_STATUS_AUDIO)
		return 0;
	writel(readl(con) | BIT(2), con);	/* PWR_ON */
	writel(readl(con) | BIT(3), con);	/* PWR_ON_2ND */
	if (readl_poll_timeout(afe->spm + SPM_PWR_STATUS, v, v & PWR_STATUS_AUDIO, 10, 10000) ||
	    readl_poll_timeout(afe->spm + SPM_PWR_STATUS2, v, v & PWR_STATUS_AUDIO, 10, 10000))
		return -ETIMEDOUT;
	writel(readl(con) & ~BIT(4), con);	/* CLK_DIS */
	writel(readl(con) & ~BIT(1), con);	/* ISO */
	writel(readl(con) | BIT(0), con);	/* RST_B */
	writel(readl(con) & ~GENMASK(11, 8), con);	/* SRAM_PDN */
	return readl_poll_timeout(con, v, !(v & GENMASK(15, 12)), 10, 10000);
}

static int afe_startup(struct snd_pcm_substream *sub, struct snd_soc_dai *dai)
{
	struct mt8163_afe *afe = snd_soc_dai_get_drvdata(dai);
	bool play = sub->stream == SNDRV_PCM_STREAM_PLAYBACK;
	int ret = 0;

	mutex_lock(&afe->lock);
	if (afe->users == 0) {
		ret = clk_bulk_prepare_enable(ARRAY_SIZE(afe->clks), afe->clks);
		if (!ret)
			ret = audio_power_on(afe);
		if (ret)
			goto out;
		upd(afe, AUDIO_TOP_CON0, BIT(2) | TOP_APB_BITS, TOP_APB_BITS);
	}
	afe->users++;
	/* AudDrv_Clk_On: DAC + DAC_PREDIS; AudDrv_ADC_Clk_On: ADC */
	upd(afe, AUDIO_TOP_CON0, play ? BIT(25) | BIT(26) : BIT(24), 0);
out:
	mutex_unlock(&afe->lock);
	return ret;
}

static void afe_shutdown(struct snd_pcm_substream *sub, struct snd_soc_dai *dai)
{
	struct mt8163_afe *afe = snd_soc_dai_get_drvdata(dai);
	bool play = sub->stream == SNDRV_PCM_STREAM_PLAYBACK;

	mutex_lock(&afe->lock);
	if (play) {
		upd(afe, ADDA_DL_SRC2_CON0, BIT(0), 0);
		upd(afe, AFE_I2S_CON1, BIT(0), 0);
		upd(afe, AUDIO_TOP_CON0, BIT(25) | BIT(26), BIT(25) | BIT(26));
	} else {
		upd(afe, ADDA_UL_SRC_CON0, BIT(0), 0);
		upd(afe, AUDIO_TOP_CON0, BIT(24), BIT(24));
	}
	if (--afe->users == 0) {
		upd(afe, ADDA_UL_DL_CON0, BIT(0), 0);
		upd(afe, AFE_DAC_CON0, BIT(0), 0);
		upd(afe, AUDIO_TOP_CON0, BIT(2), BIT(2));
		clk_bulk_disable_unprepare(ARRAY_SIZE(afe->clks), afe->clks);
	}
	mutex_unlock(&afe->lock);
}

/* DAI prepare runs before DAPM powers the codec: the ADDA link (and its clock to the PMIC) is up first */
static int afe_prepare(struct snd_pcm_substream *sub, struct snd_soc_dai *dai)
{
	struct mt8163_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int period = sub->runtime->period_size;

	if (sub->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		upd(afe, MEMIF_HD_CON, GENMASK(17, 16), 0);
		upd(afe, AFE_CONN_24BIT, BIT(3) | BIT(4), 0);
		upd(afe, AFE_DAC_CON1, GENMASK(11, 8), RATE_48K << 8);
		writel(0, afe->base + ADDA_PREDIS_CON0);	/* SetI2SDacOut(48000) */
		writel(0, afe->base + ADDA_PREDIS_CON1);
		writel(0x83001802, afe->base + ADDA_DL_SRC2_CON0);
		writel(0xf74f0000, afe->base + ADDA_DL_SRC2_CON1);
		writel(0xa | RATE_48K << 8, afe->base + AFE_I2S_CON1);	/* stock 0x90b at 44.1 kHz */
		upd(afe, ADDA_DL_SRC2_CON0, BIT(0), BIT(0));	/* SetI2SDacEnable(1) */
		upd(afe, AFE_I2S_CON1, BIT(0), BIT(0));
		upd(afe, ADDA_UL_DL_CON0, BIT(0), BIT(0));
		upd(afe, AFE_4C4, BIT(4), 0);
		writel(period, afe->base + IRQ_MCU_CNT1);
		upd(afe, IRQ_MCU_CON, GENMASK(7, 4), RATE_48K << 4);
	} else {
		upd(afe, ADDA_TOP_CON0, BIT(0), 0);	/* SetI2SAdcIn(48000) */
		upd(afe, ADDA_UL_SRC_CON0, GENMASK(20, 17), 0xf << 17);
		writel(0x03f87201, afe->base + ADDA_NEWIF_CFG0);
		upd(afe, ADDA_NEWIF_CFG1, GENMASK(11, 10), 3 << 10);
		upd(afe, MEMIF_HD_CON, GENMASK(23, 22), 0);
		upd(afe, AFE_CONN_24BIT, BIT(9) | BIT(10), 0);
		upd(afe, ADDA_UL_SRC_CON0, BIT(0), BIT(0));	/* SetI2SAdcEnable(1) */
		upd(afe, ADDA_UL_DL_CON0, BIT(0), BIT(0));
		writel(period, afe->base + IRQ_MCU_CNT2);
		upd(afe, IRQ_MCU_CON, GENMASK(11, 8), RATE_48K << 8);
	}
	upd(afe, AFE_DAC_CON0, BIT(0), BIT(0));	/* EnableAfe(1) */
	return 0;
}

static const struct snd_soc_dai_ops afe_dai_ops = {
	.startup = afe_startup, .shutdown = afe_shutdown, .prepare = afe_prepare,
};

static struct snd_soc_dai_driver afe_dai = {
	.name = "mt8163-afe-adda",
	.playback = {
		.stream_name = "ADDA Playback", .channels_min = 2, .channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000, .formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "ADDA Capture", .channels_min = 2, .channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000, .formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &afe_dai_ops,
};

static const struct snd_pcm_hardware afe_hw = {
	.info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE, .rates = SNDRV_PCM_RATE_48000, .rate_min = 48000, .rate_max = 48000,
	.channels_min = 2, .channels_max = 2,
	.buffer_bytes_max = 64 * 1024, .period_bytes_min = 256, .period_bytes_max = 32 * 1024,
	.periods_min = 2, .periods_max = 256,
};

static struct mt8163_afe *afe_of(struct snd_soc_component *c)
{
	return snd_soc_component_get_drvdata(c);
}

static int pcm_open(struct snd_soc_component *c, struct snd_pcm_substream *sub)
{
	snd_soc_set_runtime_hwparams(sub, &afe_hw);
	afe_of(c)->sub[sub->stream] = sub;
	/* the period counter is in frames; the memif moves 16-byte bursts */
	return snd_pcm_hw_constraint_step(sub->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);
}

static int pcm_close(struct snd_soc_component *c, struct snd_pcm_substream *sub)
{
	afe_of(c)->sub[sub->stream] = NULL;
	return 0;
}

static int pcm_hw_params(struct snd_soc_component *c, struct snd_pcm_substream *sub, struct snd_pcm_hw_params *p)
{
	struct mt8163_afe *afe = afe_of(c);
	dma_addr_t a = sub->runtime->dma_addr;
	size_t n = params_buffer_bytes(p);
	bool play = sub->stream == SNDRV_PCM_STREAM_PLAYBACK;

	writel(lower_32_bits(a), afe->base + (play ? AFE_DL1_BASE : AFE_VUL_BASE));
	writel(lower_32_bits(a + n - 1), afe->base + (play ? AFE_DL1_END : AFE_VUL_END));
	return 0;
}

static int pcm_trigger(struct snd_soc_component *c, struct snd_pcm_substream *sub, int cmd)
{
	struct mt8163_afe *afe = afe_of(c);
	bool play = sub->stream == SNDRV_PCM_STREAM_PLAYBACK, on;
	u32 irq = play ? BIT(0) : BIT(1);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		on = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		on = false;
		break;
	default:
		return -EINVAL;
	}
	if (play) {
		upd(afe, AFE_CONN1, BIT(21), on ? BIT(21) : 0);	/* I05 -> O03 */
		upd(afe, AFE_CONN2, BIT(6), on ? BIT(6) : 0);	/* I06 -> O04 */
		upd(afe, AFE_DAC_CON1, GENMASK(3, 0) | BIT(21), RATE_48K);	/* 48 kHz, stereo */
		upd(afe, AFE_DAC_CON0, BIT(1), on ? BIT(1) : 0);
	} else {
		upd(afe, AFE_CONN3, BIT(0) | BIT(3), on ? BIT(0) | BIT(3) : 0);	/* I03 -> O09, I04 -> O10 */
		upd(afe, AFE_DAC_CON1, GENMASK(19, 16) | BIT(27), RATE_48K << 16);
		upd(afe, AFE_DAC_CON0, BIT(3), on ? BIT(3) : 0);
	}
	upd(afe, IRQ_MCU_CON, irq, on ? irq : 0);
	if (!on)
		writel(irq, afe->base + IRQ_MCU_CLR);
	return 0;
}

static snd_pcm_uframes_t pcm_pointer(struct snd_soc_component *c, struct snd_pcm_substream *sub)
{
	struct mt8163_afe *afe = afe_of(c);
	bool play = sub->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u32 cur = readl(afe->base + (play ? AFE_DL1_CUR : AFE_VUL_CUR));
	u32 base = lower_32_bits(sub->runtime->dma_addr);

	if (cur < base || cur >= base + snd_pcm_lib_buffer_bytes(sub))
		return 0;
	return bytes_to_frames(sub->runtime, cur - base);
}

static int pcm_new(struct snd_soc_component *c, struct snd_soc_pcm_runtime *rtd)
{
	snd_pcm_set_managed_buffer_all(rtd->pcm, SNDRV_DMA_TYPE_DEV, c->dev, 32 * 1024, 64 * 1024);
	return 0;
}

static irqreturn_t afe_irq(int irq, void *data)
{
	struct mt8163_afe *afe = data;
	u32 st = readl(afe->base + IRQ_MCU_STATUS) & 0x3;

	if (!st)
		return IRQ_NONE;
	writel(st, afe->base + IRQ_MCU_CLR);
	if ((st & BIT(0)) && afe->sub[SNDRV_PCM_STREAM_PLAYBACK])
		snd_pcm_period_elapsed(afe->sub[SNDRV_PCM_STREAM_PLAYBACK]);
	if ((st & BIT(1)) && afe->sub[SNDRV_PCM_STREAM_CAPTURE])
		snd_pcm_period_elapsed(afe->sub[SNDRV_PCM_STREAM_CAPTURE]);
	return IRQ_HANDLED;
}

static const struct snd_soc_component_driver afe_component = {
	.name = "mt8163-afe",
	.open = pcm_open, .close = pcm_close, .hw_params = pcm_hw_params,
	.trigger = pcm_trigger, .pointer = pcm_pointer, .pcm_construct = pcm_new,
};

static int mt8163_afe_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt8163_afe *afe;
	struct resource *spm;
	int irq, ret;

	afe = devm_kzalloc(dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;
	mutex_init(&afe->lock);
	afe->base = devm_platform_ioremap_resource_byname(pdev, "afe");
	if (IS_ERR(afe->base))
		return PTR_ERR(afe->base);
	/* SPM is shared with other drivers: map it, do not claim it */
	spm = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm");
	if (!spm)
		return -EINVAL;
	afe->spm = devm_ioremap(dev, spm->start, resource_size(spm));
	if (!afe->spm)
		return -ENOMEM;
	afe->clks[0].id = "infra";
	afe->clks[1].id = "top";
	afe->clks[2].id = "intbus";
	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(afe->clks), afe->clks);
	if (ret)
		return dev_err_probe(dev, ret, "clocks\n");
	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	platform_set_drvdata(pdev, afe);
	ret = devm_request_irq(dev, irq, afe_irq, 0, "mt8163-afe", afe);
	if (ret)
		return ret;
	return devm_snd_soc_register_component(dev, &afe_component, &afe_dai, 1);
}

static const struct of_device_id mt8163_afe_of_match[] = {
	{ .compatible = "mediatek,mt8163-afe" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_afe_of_match);

static struct platform_driver mt8163_afe_driver = {
	.probe = mt8163_afe_probe,
	.driver = { .name = "mt8163-afe", .of_match_table = mt8163_afe_of_match },
};
module_platform_driver(mt8163_afe_driver);
MODULE_LICENSE("GPL");
