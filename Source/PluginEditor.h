#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "WebResourceProvider.h"

class CacophonicClipEditor final : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit CacophonicClipEditor(CacophonicClipProcessor &processor);
    ~CacophonicClipEditor() override;

    void paint(juce::Graphics &graphics) override;
    void resized() override;

private:
    void timerCallback() override;

    // Re-entrancy guard for the size normalisation in resized(): the
    // constrainer answers by calling setBounds(), which re-enters resized().
    bool applyingBoundsConstraint = false;

    CacophonicClipProcessor &processorRef;
    juce::WebSliderRelay trimRelay { ParameterIDs::trim };
    juce::WebSliderRelay driveRelay { ParameterIDs::drive };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };
    juce::WebSliderRelay shapeRelay { ParameterIDs::shape };
    juce::WebSliderRelay emphasisRelay { ParameterIDs::emphasis };
    juce::WebSliderRelay asymRelay { ParameterIDs::asym };
    juce::WebToggleButtonRelay boostRelay { ParameterIDs::boost };
    juce::WebToggleButtonRelay bypassRelay { ParameterIDs::bypass };
    juce::WebComboBoxRelay oversamplingRelay { ParameterIDs::oversampling };
    juce::WebComboBoxRelay emphasisModeRelay { ParameterIDs::emphasisMode };
    juce::WebToggleButtonRelay autoGainRelay { ParameterIDs::autoGain };
    juce::WebBrowserComponent webView {
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withOptionsFrom (trimRelay)
            .withOptionsFrom (driveRelay)
            .withOptionsFrom (mixRelay)
            .withOptionsFrom (shapeRelay)
            .withOptionsFrom (emphasisRelay)
            .withOptionsFrom (asymRelay)
            .withOptionsFrom (boostRelay)
            .withOptionsFrom (bypassRelay)
            .withOptionsFrom (oversamplingRelay)
            .withOptionsFrom (emphasisModeRelay)
            .withOptionsFrom (autoGainRelay)
            .withResourceProvider (WebResourceProvider::getResource)
    };

    std::unique_ptr<juce::WebSliderParameterAttachment> webTrimAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> webDriveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> webMixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> webShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> webEmphasisAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> webAsymAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> webBoostAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> webBypassAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> webOversamplingAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> webEmphasisModeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> webAutoGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CacophonicClipEditor)
};
