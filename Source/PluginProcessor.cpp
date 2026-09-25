#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <limits>

CacophonicClipProcessor::CacophonicClipProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "CacophonicClip", createParameterLayout())
{
}

CacophonicClipProcessor::~CacophonicClipProcessor() = default;

// Spec 7.1: hands the bypass parameter to the host wrapper (VST3/AU/AUv3/LV2),
// so host-side bypass drives the same APVTS bool the UI rocker reads.
juce::AudioProcessorParameter *CacophonicClipProcessor::getBypassParameter() const
{
    return parameters.getParameter(ParameterIDs::bypass);
}

//==============================================================================
void CacophonicClipProcessor::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    const auto blockSize = juce::jmax(1, maximumBlockSize);
    const auto channelCount = juce::jmax(1, getTotalNumOutputChannels());
    // Spec 12 / 12.4: hand the DSP the initial emphasisMode. The active bank
    // still starts at OFF with fade 0, so a non-OFF preset fades in on the
    // first block instead of snapping in.
    clipperDSP.prepare(sampleRate, blockSize, channelCount,
                       juce::roundToInt(parameters.getRawParameterValue(ParameterIDs::emphasisMode)->load()));
    inputCopyBuffer.setSize(getTotalNumInputChannels(), juce::jmax(1, maximumBlockSize));

    // Spec 11.2: bypass crossfade reference delay. One ring buffer per channel,
    // long enough for the worst-case oversampling latency plus one full block,
    // so the reference can be read latency samples behind the live input.
    bypassMaxLatency = clipperDSP.getMaxLatencySamples();
    bypassDelayLength = bypassMaxLatency + blockSize;
    bypassDelayBuffers.assign(static_cast<size_t>(channelCount),
                              std::vector<float>(static_cast<size_t>(bypassDelayLength), 0.0f));
    bypassDelayScratch.setSize(channelCount, blockSize);
    bypassDelayWrite = 0;

    // Report the latency of the currently selected oversampling factor right
    // away; further changes are handled in processBlock().
    reportedOversampleIndex = readClipperSettings().oversampleIndex;
    setLatencySamples(clipperDSP.getLatencySamples(reportedOversampleIndex));

    // Bypass crossfade state: start fully processed. If the parameter already
    // says bypassed, processBlock fades over 5 ms on the first block.
    bypassFadeLengthSamples = juce::jmax(1, static_cast<int>(sampleRate * bypassFadeSeconds));
    bypassFadeRemaining = 0;
    bypassDryMix = 0.0f;
    bypassFadeStep = 0.0f;
    bypassTargetState = false;
    waveformSamplesPerBin = juce::jmax(1.0, sampleRate * 5.0 / static_cast<double>(MeterSnapshot::waveformPointCount));
    waveformBinFill = 0.0;
    waveformWriteIndex = 0;
    waveformFilled = 0;
    pendingInputMin = std::numeric_limits<float>::max();
    pendingInputMax = std::numeric_limits<float>::lowest();
    pendingDrivenMin = std::numeric_limits<float>::max();
    pendingDrivenMax = std::numeric_limits<float>::lowest();
    pendingOutputMin = std::numeric_limits<float>::max();
    pendingOutputMax = std::numeric_limits<float>::lowest();
    inputLeft.store(0.0f);
    inputRight.store(0.0f);
    outputLeft.store(0.0f);
    outputRight.store(0.0f);
    const juce::SpinLock::ScopedLockType lock(meterLock);
    meterSnapshot = {};
}

void CacophonicClipProcessor::releaseResources()
{
}

bool CacophonicClipProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto &mainInput = layouts.getMainInputChannelSet();
    const auto &mainOutput = layouts.getMainOutputChannelSet();

    const auto isMonoOrStereo = [](const juce::AudioChannelSet &channels)
    {
        return channels == juce::AudioChannelSet::mono() || channels == juce::AudioChannelSet::stereo();
    };

    return isMonoOrStereo(mainInput) && mainInput == mainOutput;
}

//==============================================================================
void CacophonicClipProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                           juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    auto updatePeak = [] (const juce::AudioBuffer<float>& samples, int channel)
    {
        if (channel >= samples.getNumChannels())
            return 0.0f;

        return samples.getMagnitude(channel, 0, samples.getNumSamples());
    };

    const auto inputPeakLeft = updatePeak(buffer, 0);
    const auto inputPeakRight = updatePeak(buffer, 1);

    const auto numSamples = buffer.getNumSamples();
    const auto canCaptureHistory = numSamples > 0 && numSamples <= inputCopyBuffer.getNumSamples();
    if (canCaptureHistory)
    {
        const auto channelsToCopy = juce::jmin(buffer.getNumChannels(), inputCopyBuffer.getNumChannels());
        for (int channel = 0; channel < channelsToCopy; ++channel)
            inputCopyBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    }

    for (auto channel = getTotalNumInputChannels();
         channel < getTotalNumOutputChannels();
         ++channel)
    {
        buffer.clear(channel, 0, numSamples);
    }

    const auto clipperSettings = readClipperSettings();
    const auto bypassOn = parameters.getRawParameterValue(ParameterIDs::bypass)->load() > 0.5f;

    // Spec 11.2: keep the reported latency in sync with the oversampling choice.
    if (clipperSettings.oversampleIndex != reportedOversampleIndex)
    {
        reportedOversampleIndex = clipperSettings.oversampleIndex;
        setLatencySamples(clipperDSP.getLatencySamples(reportedOversampleIndex));
    }
    // The bypass crossfade aligns against whatever latency we just reported.
    const auto latencySamples = juce::jlimit(0, bypassMaxLatency, getLatencySamples());

    // Re-arm the 5 ms linear crossfade whenever the bypass target flips; the
    // counter keeps advancing across block boundaries until the fade lands.
    if (bypassOn != bypassTargetState)
    {
        bypassTargetState = bypassOn;
        bypassFadeRemaining = bypassFadeLengthSamples;
        bypassFadeStep = (bypassTargetState ? 1.0f : -1.0f)
            / static_cast<float>(bypassFadeLengthSamples);
    }

    const auto needsDrySignal = bypassFadeRemaining > 0 || bypassDryMix > 0.0f;

    // Same product the DSP applies at the clipper input (2x x Drive; Trim moved
    // to the post-mix output stage, spec 7.4), used to record the driven
    // envelope so the UI never has to re-estimate it from stale history.
    const auto displayGain = juce::Decibels::decibelsToGain(clipperSettings.driveDb)
        * (clipperSettings.doubleGain ? 2.0f : 1.0f);
    // While bypassed the driven envelope records the input as-is, so input,
    // driven and output all show the same pass-through signal. The output
    // envelope needs no special case: the crossfade below turns the buffer back
    // into the dry input whenever bypass is fully engaged.
    const auto drivenGain = bypassOn ? 1.0f : displayGain;
    // CLIP is a current-state indicator. Do not derive it from the five-second
    // waveform history: one clipped sample would otherwise keep it lit for the
    // entire visible history and make the lamp's meaning ambiguous.
    const auto drivenPeak = juce::jmax (inputPeakLeft, inputPeakRight) * drivenGain;

    // Feed the bypass reference ring buffers with the raw input (before the DSP
    // touches the buffer) and stash the latency-shifted copy of this block in
    // bypassDelayScratch. Writing first, then reading back latencySamples
    // behind the write head, makes latency == 0 read back exactly the input of
    // this sample. This runs every block so the ring is always warm.
    const auto delayReady = !bypassDelayBuffers.empty()
        && bypassDelayLength > 0
        && numSamples <= bypassDelayScratch.getNumSamples()
        && buffer.getNumChannels() <= bypassDelayScratch.getNumChannels();
    if (delayReady)
    {
        const auto firstSegment = juce::jmin(numSamples, bypassDelayLength - bypassDelayWrite);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto& ring = bypassDelayBuffers[static_cast<size_t>(channel)];
            const auto* input = buffer.getReadPointer(channel);
            auto* delayed = bypassDelayScratch.getWritePointer(channel);

            for (int sample = 0; sample < firstSegment; ++sample)
                ring[static_cast<size_t>(bypassDelayWrite) + static_cast<size_t>(sample)] = input[sample];
            for (int sample = firstSegment; sample < numSamples; ++sample)
                ring[static_cast<size_t>(sample - firstSegment)] = input[sample];

            for (int sample = 0; sample < numSamples; ++sample)
            {
                auto readPosition = bypassDelayWrite + sample - latencySamples;
                if (readPosition < 0)
                    readPosition += bypassDelayLength;
                else if (readPosition >= bypassDelayLength)
                    readPosition -= bypassDelayLength;
                delayed[sample] = ring[static_cast<size_t>(readPosition)];
            }
        }
        bypassDelayWrite += numSamples;
        if (bypassDelayWrite >= bypassDelayLength)
            bypassDelayWrite -= bypassDelayLength;
    }

    clipperDSP.process(buffer, clipperSettings);

    // Linear crossfade between the processed buffer and the dry input captured
    // above (5 ms, sample-accurate, runs over block boundaries). The dry side is
    // the input delayed by the reported latency (spec 11.2), falling back to the
    // plain input copy if the host exceeds the prepared block size. It needs that
    // input copy, i.e. the host respecting maximumBlockSize.
    if (needsDrySignal && canCaptureHistory)
    {
        const auto dryChannels = juce::jmin(buffer.getNumChannels(), inputCopyBuffer.getNumChannels());
        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (bypassFadeRemaining > 0)
            {
                bypassDryMix = juce::jlimit(0.0f, 1.0f, bypassDryMix + bypassFadeStep);
                if (--bypassFadeRemaining == 0)
                    bypassDryMix = bypassTargetState ? 1.0f : 0.0f;
            }

            const auto dryMix = bypassDryMix;
            if (dryMix <= 0.0f)
                continue;

            for (int channel = 0; channel < dryChannels; ++channel)
            {
                const auto processed = buffer.getSample(channel, sample);
                const auto dry = delayReady ? bypassDelayScratch.getSample(channel, sample)
                                            : inputCopyBuffer.getSample(channel, sample);
                // Exact copy at dryMix == 1 so bypassed input and output match bit for bit.
                buffer.setSample(channel, sample,
                                 dryMix >= 1.0f ? dry
                                                : processed + (dryMix * (dry - processed)));
            }
        }
    }

    const auto processedLeft = updatePeak(buffer, 0);
    const auto processedRight = updatePeak(buffer, 1);

    if (canCaptureHistory)
    {
        const auto channels = juce::jmin(buffer.getNumChannels(), inputCopyBuffer.getNumChannels());
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float inMin = std::numeric_limits<float>::max();
            float inMax = std::numeric_limits<float>::lowest();
            float drivenMin = std::numeric_limits<float>::max();
            float drivenMax = std::numeric_limits<float>::lowest();
            float outMin = std::numeric_limits<float>::max();
            float outMax = std::numeric_limits<float>::lowest();

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto inValue = inputCopyBuffer.getSample(channel, sample);
                const auto drivenValue = inValue * drivenGain;
                const auto outValue = buffer.getSample(channel, sample);
                inMin = juce::jmin(inMin, inValue);
                inMax = juce::jmax(inMax, inValue);
                drivenMin = juce::jmin(drivenMin, drivenValue);
                drivenMax = juce::jmax(drivenMax, drivenValue);
                outMin = juce::jmin(outMin, outValue);
                outMax = juce::jmax(outMax, outValue);
            }

            if (channels == 0)
                inMin = inMax = drivenMin = drivenMax = outMin = outMax = 0.0f;

            pendingInputMin = juce::jmin(pendingInputMin, inMin);
            pendingInputMax = juce::jmax(pendingInputMax, inMax);
            pendingDrivenMin = juce::jmin(pendingDrivenMin, drivenMin);
            pendingDrivenMax = juce::jmax(pendingDrivenMax, drivenMax);
            pendingOutputMin = juce::jmin(pendingOutputMin, outMin);
            pendingOutputMax = juce::jmax(pendingOutputMax, outMax);

            waveformBinFill += 1.0;
            if (waveformBinFill >= waveformSamplesPerBin)
            {
                waveformBinFill -= waveformSamplesPerBin;
                const auto bin = waveformWriteIndex;
                {
                    const juce::SpinLock::ScopedLockType lock(meterLock);
                    meterSnapshot.inputWaveformMin[static_cast<size_t>(bin)] = pendingInputMin;
                    meterSnapshot.inputWaveformMax[static_cast<size_t>(bin)] = pendingInputMax;
                    meterSnapshot.drivenWaveformMin[static_cast<size_t>(bin)] = pendingDrivenMin;
                    meterSnapshot.drivenWaveformMax[static_cast<size_t>(bin)] = pendingDrivenMax;
                    meterSnapshot.outputWaveformMin[static_cast<size_t>(bin)] = pendingOutputMin;
                    meterSnapshot.outputWaveformMax[static_cast<size_t>(bin)] = pendingOutputMax;
                    waveformWriteIndex = (bin + 1) % MeterSnapshot::waveformPointCount;
                    waveformFilled = juce::jmin(MeterSnapshot::waveformPointCount, waveformFilled + 1);
                    meterSnapshot.waveformWriteIndex = waveformWriteIndex;
                    meterSnapshot.waveformFilled = waveformFilled;
                }
                pendingInputMin = std::numeric_limits<float>::max();
                pendingInputMax = std::numeric_limits<float>::lowest();
                pendingDrivenMin = std::numeric_limits<float>::max();
                pendingDrivenMax = std::numeric_limits<float>::lowest();
                pendingOutputMin = std::numeric_limits<float>::max();
                pendingOutputMax = std::numeric_limits<float>::lowest();
            }
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(meterLock);
        meterSnapshot.inputLeft = inputPeakLeft;
        meterSnapshot.inputRight = inputPeakRight;
        meterSnapshot.outputLeft = processedLeft;
        meterSnapshot.outputRight = processedRight;
        meterSnapshot.drivenPeak = drivenPeak;
    }
    this->inputLeft.store(inputPeakLeft);
    this->inputRight.store(inputPeakRight);
    this->outputLeft.store(processedLeft);
    this->outputRight.store(processedRight);
}

MeterSnapshot CacophonicClipProcessor::getMeterSnapshot() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(meterLock);
    return meterSnapshot;
}

ClipperSettings CacophonicClipProcessor::readClipperSettings() const
{
    // Every ClipperSettings field must be listed here: partial aggregate
    // initialisation would trip -Wmissing-field-initializers.
    return {
        parameters.getRawParameterValue(ParameterIDs::trim)->load(),
        parameters.getRawParameterValue(ParameterIDs::drive)->load(),
        parameters.getRawParameterValue(ParameterIDs::mix)->load(),
        parameters.getRawParameterValue(ParameterIDs::boost)->load() > 0.5f,
        parameters.getRawParameterValue(ParameterIDs::shape)->load(),
        // Spec 12.1: emphasis is 0 ... 100 percent now, not 0 ... 10 dB.
        parameters.getRawParameterValue(ParameterIDs::emphasis)->load(),
        juce::roundToInt(parameters.getRawParameterValue(ParameterIDs::emphasisMode)->load()),
        parameters.getRawParameterValue(ParameterIDs::asym)->load(),
        // AudioParameterChoice stores the raw choice index as its value.
        juce::roundToInt(parameters.getRawParameterValue(ParameterIDs::oversampling)->load()),
        parameters.getRawParameterValue(ParameterIDs::autoGain)->load() > 0.5f
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout CacophonicClipProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::trim, 1},
        "Trim",
        juce::NormalisableRange<float>(-24.0f, 0.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String(value, 1) + " dB"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::drive, 1},
        "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String(value, 1) + " dB"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::mix, 1},
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String(juce::roundToInt(value * 100.0f)) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParameterIDs::boost, 1},
        "2x",
        false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::shape, 1},
        "Shape",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int)
                                         {
                                             if (value <= 0.02f)
                                                 return juce::String("SOFT");
                                             if (value >= 0.98f)
                                                 return juce::String("HARD");
                                             return juce::String(juce::roundToInt(value * 100.0f)) + " %";
                                         })));

    // Added last so existing parameter indices stay stable.
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParameterIDs::bypass, 1},
        "Bypass",
        false));

    // Spec 11.1: appended after bypass so existing parameter indices stay stable.
    // Spec 12.1: emphasis is now 0 ... 100 %; it scales the selected mode's
    // shelf / bell gain and corner frequency (supersedes the spec 11.1 dB range).
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::emphasis, 1},
        "Emphasis",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String(static_cast<int>(std::round(value))) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIDs::asym, 1},
        "Asymmetry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int)
                                         {
                                             return juce::String(static_cast<int>(std::round(value * 100.0f))) + " %";
                                         })));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParameterIDs::oversampling, 1},
        "Oversampling",
        juce::StringArray{"1x", "2x", "4x", "8x"},
        2));

    // Spec 12.1: appended at the very end, after oversampling.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParameterIDs::emphasisMode, 1},
        "Emphasis Mode",
        juce::StringArray{"OFF", "TAPE", "TUBE"},
        0));

    // Appended last to keep every existing parameter index stable.
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParameterIDs::autoGain, 1},
        "Auto Gain",
        false));

    return layout;
}

//==============================================================================
juce::AudioProcessorEditor *CacophonicClipProcessor::createEditor()
{
    return new CacophonicClipEditor(*this);
}

bool CacophonicClipProcessor::hasEditor() const
{
    return true;
}

const juce::String CacophonicClipProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CacophonicClipProcessor::acceptsMidi() const
{
    return false;
}

bool CacophonicClipProcessor::producesMidi() const
{
    return false;
}

bool CacophonicClipProcessor::isMidiEffect() const
{
    return false;
}

double CacophonicClipProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int CacophonicClipProcessor::getNumPrograms()
{
    return 1;
}

int CacophonicClipProcessor::getCurrentProgram()
{
    return 0;
}

void CacophonicClipProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String CacophonicClipProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void CacophonicClipProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void CacophonicClipProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void CacophonicClipProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restoredState = juce::ValueTree::fromXml(*xml);
        parameters.replaceState(restoredState);
    }
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new CacophonicClipProcessor();
}
