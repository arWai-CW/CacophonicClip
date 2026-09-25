#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <vector>

struct ClipperSettings
{
    float trimDb = 0.0f;
    float driveDb = 0.0f;
    float mix = 1.0f;
    bool doubleGain = false;
    // 0 = soft (tanh), 1 = hard clip; anything between is a linear morph.
    float shape = 0.0f;
    // Emphasis amount in percent (0 ... 100); scales the mode shelf / bell
    // gain and corner frequency (spec 12.1).
    float emphasisPercent = 0.0f;
    // Emphasis mode choice index: 0 = OFF, 1 = TAPE, 2 = TUBE (spec 12.1).
    int emphasisMode = 0;
    // Asymmetry amount (0 ... 1); scales the clip bias. See spec 11.2.
    float asym = 0.0f;
    // Oversampling choice index: 0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x.
    int oversampleIndex = 2;
    // When enabled, the output trim follows Drive inversely. This deliberately
    // excludes boost2x; Drive and Trim mirror each other one-for-one.
    bool autoGain = false;
};

class ClipperDSP
{
public:
    static constexpr auto parameterSmoothingSeconds = 0.02;

    // Spec 11.2 constants: copied verbatim from the spec, not to be retuned.
    static constexpr float kAcCoupleHz = 12.0f;
    static constexpr float kAsymBias = 0.20f;

    // Spec 12.2 mode constants: midpoints of the ranges handed in during
    // spec 11, copied verbatim, not to be retuned.
    static constexpr float kTapeHpHz = 25.0f;
    static constexpr float kTapeBumpHz = 75.0f;
    static constexpr float kTapeBumpDb = 2.25f;
    static constexpr float kTapeShelfMinHz = 2500.0f;
    static constexpr float kTapeShelfSpanHz = 1000.0f;
    static constexpr float kTapeMaxDb = 8.0f;

    static constexpr float kTubeHpHz = 115.0f;
    static constexpr float kTubeLpHz = 14000.0f;
    static constexpr float kTubeLowShelfHz = 100.0f;
    static constexpr float kTubeLowShelfDb = 3.0f;
    static constexpr float kTubeBellMinHz = 1500.0f;
    static constexpr float kTubeBellSpanHz = 1500.0f;
    static constexpr float kTubeBellQMin = 0.7f;
    static constexpr float kTubeBellQSpan = 0.3f;
    static constexpr float kTubeMaxDb = 6.0f;

    // Spec 12.2 skipping rule: the shelf / bell stage (pre and post) is not
    // built and not run while gainDb falls below this threshold.
    static constexpr float kEmphasisSkipDb = 0.01f;
    // Spec 12.2: Q for every shelf. This is the value JUCE keeps in
    // juce::dsp::IIR::ArrayCoefficients<float>::inverseRootTwo, which is a
    // private member, so the same value is restated here.
    static constexpr float kShelfQ = 0.70710678118654752440f;

    // Spec 12.4: a mode change crossfades over 5 ms.
    static constexpr double kModeCrossfadeSeconds = 0.005;

    // Emphasis mode choice indices (spec 12.1).
    static constexpr int emphasisModeOff = 0;
    static constexpr int emphasisModeTape = 1;
    static constexpr int emphasisModeTube = 2;
    static constexpr int emphasisModeCount = 3;

    // One oversampler per choice index: 0 = 1x (dummy), 1 = 2x, 2 = 4x, 3 = 8x.
    static constexpr int oversamplerCount = 4;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels,
                  int initialEmphasisMode = emphasisModeOff);
    void reset();

    void process (juce::AudioBuffer<float>& buffer,
                  const ClipperSettings& settings) noexcept;

    // Reported latency of one oversampling index, in base-rate samples (integer).
    [[nodiscard]] int getLatencySamples (int index) const;
    // Worst-case latency across all indices; sizes the bypass reference delay.
    [[nodiscard]] int getMaxLatencySamples() const;

private:
    // Biquad coefficients in juce::dsp::IIR::ArrayCoefficients order
    // {b0, b1, b2, a0, a1, a2}, normalised so a0 == 1. The difference
    // equations below therefore need no division (spec 12.3).
    using BiquadCoefficients = std::array<float, 6>;

    // Direct form 1 recursion state of one biquad stage, one channel.
    struct BiquadState
    {
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;
    };

    // One emphasis bank carries the complete pre chain (stage 1 = mode high
    // pass, stage 2 = tape shelf | tube bell) and post chain (stage 1 = the
    // exact inverse shelf / bell, stage 2 = tape head bump | tube low pass,
    // stage 3 = tube 100 Hz low shelf; tape leaves stage 3 idle), with one
    // recursion state per stage per channel. Two banks exist so a mode
    // change can crossfade without running the clipper twice (spec 12.4).
    struct ModeBank
    {
        std::array<std::vector<BiquadState>, 2> preStages;
        std::array<std::vector<BiquadState>, 3> postStages;

        void resize (size_t channels);
        void resetStates() noexcept;
    };

    // Fixed voicing coefficients (HP / LP / low shelf): they depend only on
    // fsOS, so prepare() designs one set per oversampling factor (spec 12.2).
    struct FixedCoefficients
    {
        BiquadCoefficients tapeHighPass {};
        BiquadCoefficients tapeBump {};
        BiquadCoefficients tubeHighPass {};
        BiquadCoefficients tubeLowPass {};
        BiquadCoefficients tubeLowShelf {};
    };

    // The shelf / bell pair: rebuilt once per base sample for every live
    // mode, skipped entirely while gainDb < 0.01 (spec 12.2). The post side
    // is the exact inverse of the pre side via the swap of spec 12.3, so the
    // pair cancels to 1 algebraically and is only ever designed once.
    struct DynamicCoefficients
    {
        bool tapeShelfActive = false;
        BiquadCoefficients tapePre {};
        BiquadCoefficients tapePost {};
        bool tubeBellActive = false;
        BiquadCoefficients tubePre {};
        BiquadCoefficients tubePost {};
    };

    static BiquadCoefficients normalise (const std::array<float, 6>& raw) noexcept;
    static BiquadCoefficients invertPre (const BiquadCoefficients& pre) noexcept;
    static float processStage (BiquadState& state, float input,
                               const BiquadCoefficients& coefficients) noexcept;
    static void clearStage (std::vector<BiquadState>& stage) noexcept;

    static float processPreChain (ModeBank& bank, int mode, int channel, float input,
                                  const FixedCoefficients& fixed,
                                  const DynamicCoefficients& dynamic) noexcept;
    static float processPostChain (ModeBank& bank, int mode, int channel, float input,
                                   const FixedCoefficients& fixed,
                                   const DynamicCoefficients& dynamic) noexcept;

    // Spec 12.4: clears the bank that is about to become the "new" side and
    // arms the 5 ms linear crossfade towards the given mode.
    void beginEmphasisFade (int mode) noexcept;

    float processAcCouple (int channel, float input) noexcept;

    static float clipFn (float value, float shape) noexcept;

    juce::SmoothedValue<float> smoothedTrimGain;
    juce::SmoothedValue<float> smoothedDriveGain;
    juce::SmoothedValue<float> smoothedMix;
    juce::SmoothedValue<float> smoothedDoubleGain;
    juce::SmoothedValue<float> smoothedShape;
    juce::SmoothedValue<float> smoothedEmphasis;
    juce::SmoothedValue<float> smoothedAsym;

    // JUCE's factor argument is an exponent (2^factor), and it asserts
    // isPositiveAndBelow(factor, 5): 0 adds a dummy stage (latency 0).
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, oversamplerCount> oversamplers;
    std::array<int, oversamplerCount> oversamplingFactors { 1, 2, 4, 8 };

    // Fixed mode voicing coefficients, one set per oversampling factor.
    std::array<FixedCoefficients, oversamplerCount> fixedModeCoefficients {};

    // Two emphasis banks plus the crossfade machine (spec 12.4): while a
    // fade runs, activeBankIndex holds the old chain and the other bank the
    // new one; when fade reaches 1 the active pointer swaps over.
    std::array<ModeBank, 2> modeBanks;
    int activeBankIndex = 0;
    int activeEmphasisMode = emphasisModeOff;
    int pendingEmphasisMode = emphasisModeOff;
    bool emphasisFading = false;
    float emphasisFade = 0.0f;
    // 5 ms at 48 kHz; prepare() overwrites this with the real host rate.
    int emphasisFadeLengthSamples = static_cast<int> (48000.0 * kModeCrossfadeSeconds);
    int emphasisFadeRemaining = 0;
    float emphasisFadeStep = 0.0f;

    // AC-coupling high pass (12 Hz, base rate, always on; spec 11.2).
    float acB0 = 1.0f;
    float acB1 = 0.0f;
    float acA1 = 0.0f;

    // Filter states of the AC coupling stage, one set per channel.
    std::vector<float> acX1, acY1;

    double preparedSampleRate = 0.0;
    int preparedChannels = 0;
    int preparedBlockSize = 0;
    bool prepared = false;
};
