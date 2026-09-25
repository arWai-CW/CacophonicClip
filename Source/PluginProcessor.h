#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ClipperDSP.h"

#include <array>
#include <vector>

namespace ParameterIDs
{
    inline constexpr auto trim = "trim";
    inline constexpr auto drive = "drive";
    inline constexpr auto mix = "mix";
    inline constexpr auto boost = "boost2x";
    inline constexpr auto shape = "shape";
    inline constexpr auto bypass = "bypass";
    // Spec 11.1: appended after bypass so existing parameter indices stay stable.
    inline constexpr auto emphasis = "emphasis";
    inline constexpr auto asym = "asym";
    inline constexpr auto oversampling = "oversampling";
    // Spec 12.1: appended after oversampling, still at the tail of the layout.
    inline constexpr auto emphasisMode = "emphasisMode";
    // Appended last so all existing parameter indices stay stable.
    inline constexpr auto autoGain = "autoGain";
}

struct MeterSnapshot
{
    // 512 bins covering ~5 seconds of audio (min/max envelope per bin).
    static constexpr int waveformPointCount = 512;

    float inputLeft = 0.0f;
    float inputRight = 0.0f;
    float outputLeft = 0.0f;
    float outputRight = 0.0f;
    std::array<float, waveformPointCount> inputWaveformMin {};
    std::array<float, waveformPointCount> inputWaveformMax {};
    std::array<float, waveformPointCount> drivenWaveformMin {};
    std::array<float, waveformPointCount> drivenWaveformMax {};
    std::array<float, waveformPointCount> outputWaveformMin {};
    std::array<float, waveformPointCount> outputWaveformMax {};
    int waveformWriteIndex = 0;
    int waveformFilled = 0;
};

class CacophonicClipProcessor final : public juce::AudioProcessor
{
public:
    CacophonicClipProcessor();
    ~CacophonicClipProcessor() override;

    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &buffer,
                      juce::MidiBuffer &midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorParameter *getBypassParameter() const override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState &getAPVTS() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState &getAPVTS() const noexcept { return parameters; }
    MeterSnapshot getMeterSnapshot() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    ClipperSettings readClipperSettings() const;

    juce::AudioProcessorValueTreeState parameters;
    ClipperDSP clipperDSP;

    // Bypass crossfade state (spec 7.3): a 5 ms linear fade between the
    // processed signal and the dry input so toggling bypass never clicks.
    // bypassDryMix runs 0 (fully processed) -> 1 (fully bypassed); the counter
    // advances per sample and survives block boundaries. Cleared in
    // prepareToPlay().
    static constexpr double bypassFadeSeconds = 0.005;
    int bypassFadeLengthSamples = 220;
    int bypassFadeRemaining = 0;
    float bypassDryMix = 0.0f;
    float bypassFadeStep = 0.0f;
    bool bypassTargetState = false;

    // Spec 11.2: the bypass crossfade fades against the input delayed by the
    // reported oversampling latency, so the dry reference stays time-aligned
    // with the wet signal (otherwise the fade combs). One circular buffer per
    // channel, length maxLatency + maximumBlockSize, fed every block in
    // processBlock() before the DSP runs. Plain std::vector ring buffers on
    // purpose: juce::dsp::DelayLine misbehaves at delay 0.
    std::vector<std::vector<float>> bypassDelayBuffers;
    juce::AudioBuffer<float> bypassDelayScratch; // latency-compensated copy of the current block
    int bypassDelayLength = 0;
    int bypassDelayWrite = 0;
    int bypassMaxLatency = 0;

    // Currently reported oversampling index; drives setLatencySamples() when
    // the oversampling choice changes (spec 11.2).
    int reportedOversampleIndex = -1;

    std::atomic<float> inputLeft { 0.0f };
    std::atomic<float> inputRight { 0.0f };
    std::atomic<float> outputLeft { 0.0f };
    std::atomic<float> outputRight { 0.0f };
    juce::AudioBuffer<float> inputCopyBuffer;
    double waveformSamplesPerBin = 240.0;
    double waveformBinFill = 0.0;
    int waveformWriteIndex = 0;
    int waveformFilled = 0;
    float pendingInputMin = 0.0f;
    float pendingInputMax = 0.0f;
    float pendingDrivenMin = 0.0f;
    float pendingDrivenMax = 0.0f;
    float pendingOutputMin = 0.0f;
    float pendingOutputMax = 0.0f;
    juce::SpinLock meterLock;
    MeterSnapshot meterSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CacophonicClipProcessor)
};
