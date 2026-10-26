"""audio: the stock speaker playback path as one ordered register sequence (MT8163 AFE + MT6323 audio).

Order and arguments follow the vendor mt_soc_pcm_dl1.c open/prepare/start (enum values compiled from the vendor
headers); every step's registers come from the stock binary (`trace`). 48 kHz, S16_LE, stereo, period 1024.
"""
import struct
from pathlib import Path

from .trace import trace

RATE, PERIOD = 48000, 1024
DL1, I2S, I2S_OUT_DAC = 0, 4, 12
CONNECTION, I05, I06, O03, O04 = 1, 5, 6, 3, 4
IRQ1, WLEN16, OUT16 = 0, 0, 0

STEPS = [
    ("open: EMI clock", "AudDrv_Emi_Clk_On", ()),
    ("open: analog clock", "AudDrv_ANA_Clk_On", ()),
    ("open: AFE clock", "AudDrv_Clk_On", ()),
    ("prepare: DL1 16-bit fetch", "SetMemIfFetchFormatPerSample", (DL1, WLEN16)),
    ("prepare: O03 16-bit", "SetoutputConnectionFormat", (OUT16, O03)),
    ("prepare: O04 16-bit", "SetoutputConnectionFormat", (OUT16, O04)),
    ("prepare: I2S rate", "SetSampleRate", (I2S, RATE)),
    ("prepare: I2S_OUT_DAC path", "SetMemoryPathEnable", (I2S_OUT_DAC, 1)),
    ("prepare: I2S DAC out", "SetI2SDacOut", (RATE, 0, 0)),
    ("prepare: I2S DAC enable", "SetI2SDacEnable", (1,)),
    ("prepare: IRQ1 counter", "SetIrqMcuCounter", (IRQ1, PERIOD)),
    ("prepare: IRQ1 rate", "SetIrqMcuSampleRate", (IRQ1, RATE)),
    ("prepare: AFE on", "EnableAfe", (1,)),
    ("start: I05 -> O03", "SetConnection", (CONNECTION, I05, O03)),
    ("start: I06 -> O04", "SetConnection", (CONNECTION, I06, O04)),
    ("start: IRQ1 on", "SetIrqEnable", (IRQ1, 1)),
    ("start: DL1 rate", "SetSampleRate", (DL1, RATE)),
    ("start: DL1 stereo", "SetChannels", (DL1, 2)),
    ("start: DL1 on", "SetMemoryPathEnable", (DL1, 1)),
    ("start: AFE on", "EnableAfe", (1,)),
    ("codec: speaker on", "Speaker_Amp_Change", (1,)),
]


# capture: mtk_capture_pcm_open/trigger (VUL = memif 2, I03/I04 -> O09/O10, IRQ2); SetI2SAdcIn takes a struct and is
# decoded by hand in mt8163-afe.c (48 kHz: UL_SRC_CON0 rate 0xf, NEWIF_CFG0 0x03f87201, NEWIF_CFG1 3 << 10)
VUL, ADC1, ADC2, I03, I04, O09, O10, IRQ2, I2S_IN_ADC = 2, 13, 14, 3, 4, 9, 10, 1, 14
CAPTURE = [
    ("open: ADC clock", "AudDrv_ADC_Clk_On", ()),
    ("trigger: VUL 16-bit fetch", "SetMemIfFetchFormatPerSample", (VUL, WLEN16)),
    ("trigger: O09 16-bit", "SetoutputConnectionFormat", (OUT16, O09)),
    ("trigger: O10 16-bit", "SetoutputConnectionFormat", (OUT16, O10)),
    ("trigger: I2S ADC enable", "SetI2SAdcEnable", (1,)),
    ("trigger: I03 -> O09", "SetConnection", (CONNECTION, I03, O09)),
    ("trigger: I04 -> O10", "SetConnection", (CONNECTION, I04, O10)),
    ("trigger: IRQ2 counter", "SetIrqMcuCounter", (IRQ2, PERIOD)),
    ("trigger: IRQ2 rate", "SetIrqMcuSampleRate", (IRQ2, RATE)),
    ("trigger: IRQ2 on", "SetIrqEnable", (IRQ2, 1)),
    ("trigger: VUL rate", "SetSampleRate", (VUL, RATE)),
    ("trigger: VUL stereo", "SetChannels", (VUL, 2)),
    ("codec: ADC1 on", "TurnOnADcPowerACC", (ADC1, 1)),
    ("codec: ADC2 on", "TurnOnADcPowerACC", (ADC2, 1)),
    ("codec: ADC2 off", "TurnOnADcPowerACC", (ADC2, 0)),
    ("codec: ADC1 off", "TurnOnADcPowerACC", (ADC1, 0)),
]
HEADPHONE = [("codec: headphone on", "Audio_Amp_Change.isra.15", (1,)),
             ("codec: headphone off", "Audio_Amp_Change.isra.15", (0,)),
             ("codec: speaker off", "Speaker_Amp_Change", (0,))]

# codec state (mCodec_data, kzalloc'd at probe) is reached through this global: give it a zeroed struct (no other
# path active, mic source 0) with the mic PGA at 3, and the UL rate global at 48 kHz
CODEC_PTR, UL_RATE, STATE = 0xFFFFFFC001099748, 0xFFFFFFC000E40C8C, 0x71000000


def _mem() -> dict[int, bytes]:
    st = bytearray(0x1000)
    st[52:60] = struct.pack("<II", 3, 3)
    return {CODEC_PTR: struct.pack("<Q", STATE), STATE: bytes(st), UL_RATE: struct.pack("<I", RATE)}


def recipe(steps=STEPS, what="speaker playback") -> str:
    lines = [f"# stock {what}, {RATE} Hz S16_LE stereo; Afe_* offsets from 0x11220000, Ana_* = MT6323"]
    for title, fn, args in steps:
        lines.append(f"## {title}: {fn}{args}")
        follow = ("AudDrv_Clk_On", "AudDrv_ANA_Clk_On", "AudDrv_Emi_Clk_On", "AudDrv_ADC_Clk_On") \
            if fn.startswith("AudDrv_") else ()
        for op in trace(fn, args=args, follow=follow, mem=_mem()):
            if op[0] not in ("__const_udelay", "STOP"):
                lines.append("  " + " ".join(op))
    return "\n".join(lines) + "\n"


def write() -> list[Path]:
    out = []
    for name, steps, what in (("speaker", STEPS, "speaker playback"), ("capture", CAPTURE, "mic capture"),
                              ("headphone", HEADPHONE, "headphone playback")):
        path = Path(f"src/re/golden/audio-{name}.txt")
        path.write_text(recipe(steps, what))
        out.append(path)
    return out


if __name__ == "__main__":
    print(*write(), sep="\n")
