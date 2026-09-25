#include "ClipperDSP.h"

#include <algorithm>
#include <cmath>

// Spec 12.2 / 12.3: the design functions live in juce::dsp::IIR (the spec
// writes juce::dsp::ArrayCoefficients, this is the actual location).
using ArrayCoefficients = juce::dsp::IIR::ArrayCoefficients<float>;

void ClipperDSP::ModeBank::resize (size_t channels)
{
    for (auto& stage : preStages)
        stage.assign (channels, BiquadState {});
    for (auto& stage : postStages)
        stage.assign (channels, BiquadState {});
}

void ClipperDSP::ModeBank::resetStates() noexcept
{
    for (auto& stage : preStages)
        std::fill (stage.begin(), stage.end(), BiquadState {});
    for (auto& stage : postStages)
        std::fill (stage.begin(), stage.end(), BiquadState {});
}

void ClipperDSP::prepare (double sampleRate, int maximumBlockSize, int numChannels,
                          int initialEmphasisMode)
{
    preparedChannels = juce::jmax (1, numChannels);
    preparedBlockSize = juce::jmax (1, maximumBlockSize);
    preparedSampleRate = sampleRate;

    smoothedTrimGain.reset (sampleRate, parameterSmoothingSeconds);
    smoothedDriveGain.reset (sampleRate, parameterSmoothingSeconds);
    smoothedMix.reset (sampleRate, parameterSmoothingSeconds);
    smoothedDoubleGain.reset (sampleRate, parameterSmoothingSeconds);
    smoothedShape.reset (sampleRate, parameterSmoothingSeconds);
    smoothedEmphasis.reset (sampleRate, parameterSmoothingSeconds);
    smoothedAsym.reset (sampleRate, parameterSmoothingSeconds);

    // Spec 12.4: the mode crossfade walks 0.005 * sampleRate base samples.
    emphasisFadeLengthSamples = juce::jmax (1, static_cast<int> (
        std::lround (sampleRate * kModeCrossfadeSeconds)));

    const auto channels = static_cast<size_t> (preparedChannels);
    modeBanks[0].resize (channels);
    modeBanks[1].resize (channels);

    for (int index = 0; index < oversamplerCount; ++index)
    {
        auto& oversampler = oversamplers[static_cast<size_t> (index)];
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (preparedChannels),
            static_cast<size_t> (index),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,   // isMaxQuality: -90 dB up / -75 dB down stopbands
            true);  // useIntegerLatency
        oversampler->initProcessing (static_cast<size_t> (preparedBlockSize));
        (*oversampler).reset();

        const auto oversampledRate = static_cast<float> (
            sampleRate * static_cast<double> (oversamplingFactors[static_cast<size_t> (index)]));

        // Spec 12.2: the HP / LP / low shelf stages have fixed frequencies
        // and only depend on fsOS, so each oversampling factor gets one
        // precomputed set, exactly like the kappa / gamma / g set of spec 11.2.
        auto& fixed = fixedModeCoefficients[static_cast<size_t> (index)];
        fixed.tapeHighPass = normalise (
            ArrayCoefficients::makeHighPass (oversampledRate, kTapeHpHz));
        fixed.tapeBump = normalise (
            ArrayCoefficients::makeLowShelf (oversampledRate, kTapeBumpHz, kShelfQ,
                                             juce::Decibels::decibelsToGain (kTapeBumpDb)));
        fixed.tubeHighPass = normalise (
            ArrayCoefficients::makeHighPass (oversampledRate, kTubeHpHz));
        fixed.tubeLowPass = normalise (
            ArrayCoefficients::makeLowPass (oversampledRate, kTubeLpHz));
        fixed.tubeLowShelf = normalise (
            ArrayCoefficients::makeLowShelf (oversampledRate, kTubeLowShelfHz, kShelfQ,
                                             juce::Decibels::decibelsToGain (kTubeLowShelfDb)));
    }

    // AC-coupling high pass, computed once at the base rate (spec 11.2).
    const auto kappaHP = 1.0f / std::tan (juce::MathConstants<float>::pi * kAcCoupleHz / static_cast<float> (sampleRate));
    const auto gammaHP = (1.0f - kappaHP) / (1.0f + kappaHP);
    const auto gHP = 1.0f / (1.0f + kappaHP);
    acB0 = 1.0f - gHP;
    acB1 = gammaHP - gHP;
    acA1 = gammaHP;

    acX1.assign (channels, 0.0f);
    acY1.assign (channels, 0.0f);

    reset();

    // prepareToPlay() hands over the mode the parameter already asks for
    // (spec 12 / 12.4): the active bank still starts at OFF with fade 0, so
    // the first process() fades the requested mode in from a silent chain.
    pendingEmphasisMode = juce::jlimit (emphasisModeOff, emphasisModeCount - 1, initialEmphasisMode);
    prepared = true;
}

void ClipperDSP::reset()
{
    smoothedTrimGain.setCurrentAndTargetValue (1.0f);
    smoothedDriveGain.setCurrentAndTargetValue (1.0f);
    smoothedMix.setCurrentAndTargetValue (1.0f);
    smoothedDoubleGain.setCurrentAndTargetValue (1.0f);
    smoothedShape.setCurrentAndTargetValue (0.0f);
    smoothedEmphasis.setCurrentAndTargetValue (0.0f);
    smoothedAsym.setCurrentAndTargetValue (0.0f);

    std::fill (acX1.begin(), acX1.end(), 0.0f);
    std::fill (acY1.begin(), acY1.end(), 0.0f);

    for (auto& bank : modeBanks)
        bank.resetStates();

    activeBankIndex = 0;
    activeEmphasisMode = emphasisModeOff;
    pendingEmphasisMode = emphasisModeOff;
    emphasisFading = false;
    emphasisFade = 0.0f;
    emphasisFadeRemaining = 0;
    emphasisFadeStep = 0.0f;

    for (const auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            (*oversampler).reset();
}

int ClipperDSP::getLatencySamples (int index) const
{
    if (! juce::isPositiveAndBelow (index, oversamplerCount))
        return 0;

    const auto& oversampler = oversamplers[static_cast<size_t> (index)];
    if (oversampler == nullptr)
        return 0;

    return static_cast<int> (std::lround (oversampler->getLatencyInSamples()));
}

int ClipperDSP::getMaxLatencySamples() const
{
    auto maxLatency = 0;

    for (const auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            maxLatency = juce::jmax (maxLatency,
                                     static_cast<int> (std::lround (oversampler->getLatencyInSamples())));

    return maxLatency;
}

float ClipperDSP::clipFn (float value, float shape) noexcept
{
    const auto softClipped = std::tanh (value);
    const auto hardClipped = juce::jlimit (-1.0f, 1.0f, value);
    // Morph between tanh (shape = 0) and hard clip (shape = 1). Both curves are
    // monotonic, so the blend stays monotonic too.
    return softClipped + (shape * (hardClipped - softClipped));
}

ClipperDSP::BiquadCoefficients ClipperDSP::normalise (const std::array<float, 6>& raw) noexcept
{
    // Fold a0 into the numerator and denominator so both difference
    // equations can run with a0 == 1 (spec 12.3).
    const auto a0 = raw[3];
    return { raw[0] / a0, raw[1] / a0, raw[2] / a0, 1.0f, raw[4] / a0, raw[5] / a0 };
}

ClipperDSP::BiquadCoefficients ClipperDSP::invertPre (const BiquadCoefficients& pre) noexcept
{
    // Spec 12.3: the exact inverse of N(z) = N(z) / D(z) is D(z) / N(z), so
    // the numerator and the denominator swap. pre is already normalised
    // (a0 == 1), giving post = { a0/b0, a1/b0, a2/b0, 1, b1/b0, b2/b0 }.
    const auto invB0 = 1.0f / pre[0];
    return { invB0,
             pre[4] * invB0,
             pre[5] * invB0,
             1.0f,
             pre[1] * invB0,
             pre[2] * invB0 };
}

float ClipperDSP::processStage (BiquadState& state, float input,
                                const BiquadCoefficients& coefficients) noexcept
{
    // Direct form 1 with a0 already folded in (spec 12.3):
    // y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2.
    const auto output = (coefficients[0] * input)
                      + (coefficients[1] * state.x1)
                      + (coefficients[2] * state.x2)
                      - (coefficients[4] * state.y1)
                      - (coefficients[5] * state.y2);
    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = output;
    return output;
}

void ClipperDSP::clearStage (std::vector<BiquadState>& stage) noexcept
{
    // Spec 12.2: a skipped shelf / bell stage keeps its state cleared, so
    // re-enabling it always restarts from a zeroed recursion.
    std::fill (stage.begin(), stage.end(), BiquadState {});
}

float ClipperDSP::processPreChain (ModeBank& bank, int mode, int channel, float input,
                                   const FixedCoefficients& fixed,
                                   const DynamicCoefficients& dynamic) noexcept
{
    // Spec 12.2: OFF runs nothing at all; the input passes straight through.
    if (mode == emphasisModeOff)
        return input;

    const auto index = static_cast<size_t> (channel);

    // Stage 1: mode high pass (voicing, on whenever the mode is on).
    auto output = processStage (bank.preStages[0][index], input,
                                mode == emphasisModeTape ? fixed.tapeHighPass
                                                         : fixed.tubeHighPass);

    // Stage 2: tape shelf | tube bell, skipped while gainDb < 0.01.
    const auto stageActive = mode == emphasisModeTape ? dynamic.tapeShelfActive
                                                      : dynamic.tubeBellActive;
    if (stageActive)
        output = processStage (bank.preStages[1][index], output,
                               mode == emphasisModeTape ? dynamic.tapePre
                                                        : dynamic.tubePre);

    return output;
}

float ClipperDSP::processPostChain (ModeBank& bank, int mode, int channel, float input,
                                    const FixedCoefficients& fixed,
                                    const DynamicCoefficients& dynamic) noexcept
{
    if (mode == emphasisModeOff)
        return input;

    const auto index = static_cast<size_t> (channel);
    const auto isTape = mode == emphasisModeTape;

    auto output = input;

    // Stage 1: exact inverse of the shelf / bell (the swap of spec 12.3),
    // skipped together with its pre counterpart (spec 12.2).
    const auto stageActive = isTape ? dynamic.tapeShelfActive : dynamic.tubeBellActive;
    if (stageActive)
        output = processStage (bank.postStages[0][index], output,
                               isTape ? dynamic.tapePost : dynamic.tubePost);

    // Stage 2: tape head bump (75 Hz low shelf) | tube 14 kHz low pass.
    output = processStage (bank.postStages[1][index], output,
                           isTape ? fixed.tapeBump : fixed.tubeLowPass);

    // Stage 3: tube only, 100 Hz low shelf. Tape leaves the stage idle.
    if (! isTape)
        output = processStage (bank.postStages[2][index], output, fixed.tubeLowShelf);

    return output;
}

void ClipperDSP::beginEmphasisFade (int mode) noexcept
{
    // Spec 12.4: the bank that is about to become the "new" side is cleared
    // first, then the 5 ms linear fade is armed from 0.
    modeBanks[static_cast<size_t> (1 - activeBankIndex)].resetStates();
    pendingEmphasisMode = mode;
    emphasisFading = true;
    emphasisFade = 0.0f;
    emphasisFadeRemaining = emphasisFadeLengthSamples;
    emphasisFadeStep = 1.0f / static_cast<float> (emphasisFadeLengthSamples);
}

float ClipperDSP::processAcCouple (int channel, float input) noexcept
{
    const auto index = static_cast<size_t> (channel);
    const auto output = (acB0 * input)
                      + (acB1 * acX1[index])
                      - (acA1 * acY1[index]);
    acX1[index] = input;
    acY1[index] = output;
    return output;
}

void ClipperDSP::process (juce::AudioBuffer<float>& buffer,
                          const ClipperSettings& settings) noexcept
{
    if (! prepared || buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const auto osIndex = juce::jlimit (0, oversamplerCount - 1, settings.oversampleIndex);
    auto& oversampler = *oversamplers[static_cast<size_t> (osIndex)];
    const auto factor = oversamplingFactors[static_cast<size_t> (osIndex)];
    const auto& fixed = fixedModeCoefficients[static_cast<size_t> (osIndex)];

    const auto trimTarget = juce::Decibels::decibelsToGain (settings.trimDb);
    const auto driveGain = juce::Decibels::decibelsToGain (settings.driveDb);
    const auto targetDoubleGain = settings.doubleGain ? 2.0f : 1.0f;

    smoothedTrimGain.setTargetValue (trimTarget);
    smoothedDriveGain.setTargetValue (driveGain);
    smoothedMix.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.mix));
    smoothedDoubleGain.setTargetValue (targetDoubleGain);
    smoothedShape.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.shape));
    smoothedEmphasis.setTargetValue (juce::jlimit (0.0f, 100.0f, settings.emphasisPercent));
    smoothedAsym.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.asym));

    // Spec 12.4: arm (or retarget) the 5 ms mode crossfade once per block.
    const auto requestedMode = juce::jlimit (emphasisModeOff, emphasisModeCount - 1,
                                             settings.emphasisMode);
    const auto effectiveMode = emphasisFading ? pendingEmphasisMode : activeEmphasisMode;
    if (requestedMode != effectiveMode)
    {
        if (emphasisFading)
        {
            // A second change inside a running fade is not covered by the
            // spec: hand the bank being faded in over to the active side, so
            // the restarted fade starts from the chain that is closest to
            // what is already audible instead of jumping back to the old one.
            activeBankIndex = 1 - activeBankIndex;
            activeEmphasisMode = pendingEmphasisMode;
        }
        beginEmphasisFade (requestedMode);
    }

    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (buffer.getNumChannels(), preparedChannels);
    jassert (numSamples <= preparedBlockSize);

    // 1. Upsample into the oversampler's own (writeable) block.
    const juce::dsp::AudioBlock<const float> inputBlock (buffer.getArrayOfReadPointers (),
                                                         static_cast<size_t> (channels),
                                                         static_cast<size_t> (numSamples));
    auto upBlock = oversampler.processSamplesUp (inputBlock);
    jassert (upBlock.getNumSamples () == static_cast<size_t> (numSamples * factor));

    // fsOS of the current oversampling factor; the shelf / bell pair below is
    // re-designed from it once per base sample (spec 12.2).
    const auto oversampledRate = static_cast<float> (
        preparedSampleRate * static_cast<double> (factor));

    // 2. Waveshaping at the oversampled rate. Smoothers advance exactly once
    //    per base sample, never per oversampled sample, so the 20 ms time
    //    constants stay 20 ms of real time regardless of the factor; the inner
    //    loop just reuses that one parameter frame.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Spec 12.4: the crossfade advances once per base sample. When it
        // lands on 1 the active pointer moves to the new bank, whose steady
        // state below then runs on its own (a lerp at fade == 1 is exactly
        // the new chain, so the swap does not change the output).
        if (emphasisFading)
        {
            emphasisFade = juce::jlimit (0.0f, 1.0f, emphasisFade + emphasisFadeStep);
            if (--emphasisFadeRemaining <= 0)
            {
                activeBankIndex = 1 - activeBankIndex;
                activeEmphasisMode = pendingEmphasisMode;
                emphasisFading = false;
                emphasisFade = 0.0f;
            }
        }

        const auto drive = smoothedDriveGain.getNextValue();
        const auto doubleGain = smoothedDoubleGain.getNextValue();
        const auto mix = smoothedMix.getNextValue();
        const auto shape = smoothedShape.getNextValue();
        const auto emphasisPercent = smoothedEmphasis.getNextValue();
        const auto asym = smoothedAsym.getNextValue();

        const auto bias = asym * kAsymBias;
        const auto t = clipFn (bias, shape);
        const auto invNorm = 1.0f / (1.0f + t);
        const auto emphasisAmount = juce::jlimit (0.0f, 1.0f, emphasisPercent * 0.01f);

        // Which banks run this base sample: the active one always, plus the
        // bank being faded in while a crossfade runs (spec 12.4). The idle
        // bank is not touched at all in the steady state.
        const bool crossfading = emphasisFading;
        std::array<ModeBank*, 2> liveBanks { &modeBanks[static_cast<size_t> (activeBankIndex)],
                                             &modeBanks[static_cast<size_t> (activeBankIndex)] };
        std::array<int, 2> liveModes { activeEmphasisMode, activeEmphasisMode };
        const auto fade = emphasisFade;
        if (crossfading)
        {
            liveBanks[1] = &modeBanks[static_cast<size_t> (1 - activeBankIndex)];
            liveModes[1] = pendingEmphasisMode;
        }

        // Spec 12.2: rebuild the shelf / bell pair once per base sample for
        // each live mode, and skip the whole stage (state included) while
        // gainDb < 0.01. With EMPH = 0 % this is genuinely zero work rather
        // than an identity biquad accumulating float error.
        DynamicCoefficients dynamic;
        const auto liveBankCount = crossfading ? 2 : 1;
        for (int live = 0; live < liveBankCount; ++live)
        {
            const auto liveIndex = static_cast<size_t> (live);
            if (liveModes[liveIndex] == emphasisModeTape)
            {
                const auto gainDb = emphasisAmount * kTapeMaxDb;
                if (gainDb < kEmphasisSkipDb)
                {
                    clearStage (liveBanks[liveIndex]->preStages[1]);
                    clearStage (liveBanks[liveIndex]->postStages[0]);
                }
                else
                {
                    const auto shelfHz = kTapeShelfMinHz + (kTapeShelfSpanHz * emphasisAmount);
                    dynamic.tapePre = normalise (ArrayCoefficients::makeHighShelf (
                        oversampledRate, shelfHz, kShelfQ,
                        juce::Decibels::decibelsToGain (gainDb)));
                    dynamic.tapePost = invertPre (dynamic.tapePre);
                    dynamic.tapeShelfActive = true;
                }
            }
            else if (liveModes[liveIndex] == emphasisModeTube)
            {
                const auto gainDb = emphasisAmount * kTubeMaxDb;
                if (gainDb < kEmphasisSkipDb)
                {
                    clearStage (liveBanks[liveIndex]->preStages[1]);
                    clearStage (liveBanks[liveIndex]->postStages[0]);
                }
                else
                {
                    const auto bellHz = kTubeBellMinHz + (kTubeBellSpanHz * emphasisAmount);
                    const auto bellQ = kTubeBellQMin + (kTubeBellQSpan * emphasisAmount);
                    dynamic.tubePre = normalise (ArrayCoefficients::makePeakFilter (
                        oversampledRate, bellHz, bellQ,
                        juce::Decibels::decibelsToGain (gainDb)));
                    dynamic.tubePost = invertPre (dynamic.tubePre);
                    dynamic.tubeBellActive = true;
                }
            }
        }

        const auto wetGain = drive * doubleGain;
        const auto oversampledStart = sample * factor;

        for (int k = 0; k < factor; ++k)
        {
            const auto oversampledSample = oversampledStart + k;

            for (int channel = 0; channel < channels; ++channel)
            {
                auto* data = upBlock.getChannelPointer (static_cast<size_t> (channel));
                // Dry reference for the mix: the upsampled input before any
                // emphasis, read before the in-place overwrite below.
                const auto up = data[oversampledSample];
                const auto driven = up * wetGain;

                // Spec 12.4: both banks' pre chains run during a fade, then
                // the clipper runs exactly once on the blended pre output.
                auto preOut = processPreChain (*liveBanks[0], liveModes[0], channel,
                                               driven, fixed, dynamic);
                if (crossfading)
                {
                    const auto fadedPre = processPreChain (*liveBanks[1], liveModes[1], channel,
                                                           driven, fixed, dynamic);
                    preOut += fade * (fadedPre - preOut);
                }

                const auto clipped = (clipFn (preOut + bias, shape) - t) * invNorm;

                auto wet = processPostChain (*liveBanks[0], liveModes[0], channel,
                                             clipped, fixed, dynamic);
                if (crossfading)
                {
                    const auto fadedPost = processPostChain (*liveBanks[1], liveModes[1], channel,
                                                             clipped, fixed, dynamic);
                    wet += fade * (fadedPost - wet);
                }

                data[oversampledSample] = up + (mix * (wet - up));
            }
        }
    }

    // 3. Back down to the base rate.
    juce::dsp::AudioBlock<float> outputBlock (buffer.getArrayOfWritePointers (),
                                              static_cast<size_t> (channels),
                                              static_cast<size_t> (numSamples));
    oversampler.processSamplesDown (outputBlock);

    // 4. AC-coupling (removes the DC the bias introduces, even harmonics stay)
    //    followed by the post-stage output trim, once per base sample.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto trimGain = smoothedTrimGain.getNextValue();

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            data[sample] = processAcCouple (channel, data[sample]) * trimGain;
        }
    }
}
