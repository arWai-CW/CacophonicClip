# Cacophonic Clip

**A precision clipper.** Decide exactly how peaks get flattened, from a soft tanh knee to a hard brickwall, then land the level without the guesswork.

## DOWNLOADS

[Latest release (macOS and Windows)](https://github.com/arWai-CW/CacophonicClip/releases/latest)

macOS 是 `.dmg`：掛載後對 `安裝 Cacophonic Clip.app` 按右鍵 → 打開 → 打開。
外掛沒有 Apple 開發者簽章，第一次打開會被 Gatekeeper 擋，完整步驟在 dmg 裡的 `INSTALL.txt`。

## AVAILABLE FORMATS

| Formats    | OS              | Architecture                        |
| ---------- | --------------- | ----------------------------------- |
| VST3 / AU  | macOS           | Intel and Apple Silicon (universal) |
| VST3 / AAX | Windows 10 - 11 | 64-bit                              |

## HOW IT WORKS

```
IN
|
|   1   DRIVE + 2x        input gain, how hard you hit the clipper
|   2   EMPHASIS (pre)    TAPE / TUBE voicing before the clip
|   3   CLIP              SHAPE draws the curve, ASYMMETRY biases it
|   4   EMPHASIS (post)   rolls off what stage 2 added
|   5   MIX               dry and clipped blended
|
|       stages 1 to 5 run at 1x / 2x / 4x / 8x oversampling
|
|   6   TRIM              DC cleaned up, then the final output level
|
OUT
```

## FEATURES

- **1 DRIVE + 2x**: 0 to +24 dB into the clipper. 2x doubles it.
- **2 / 4 EMPHASIS**: TAPE or TUBE voicing, 0 to 100 %, wrapped around the clip.
- **3 SHAPE**: SOFT tanh rounding to HARD brickwall. The transfer curve follows it.
- **3 ASYMMETRY**: biases positive and negative peaks apart for even harmonic colour.
- **Oversampling**: 1x to 8x, default 4x, to keep aliasing out.
- **5 MIX**: dry and clipped blend for parallel clipping.
- **6 TRIM**: -24 to 0 dB, the final output level.
- **A-GAIN**: DRIVE up, TRIM down. More saturation, same output level.
- **BYPASS** and a red **CLIP lamp**: red lights while the signal is clipped.

**On screen**: live transfer curve, stereo LED meters, and a waveform strip with clipped sections marked in red.

## QUICK START

1. Push DRIVE (add 2x if you want more) until the CLIP lamp flickers
2. Round or sharpen the peaks with SHAPE
3. Pull MIX back until it sits right in the mix
4. Set final level with TRIM
5. Toggle BYPASS for a fair before and after
