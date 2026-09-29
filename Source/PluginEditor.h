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
            // Windows' default backend is the IE/ActiveX WebBrowser control,
            // which has no resource provider support at all: it would navigate
            // getResourceProviderRoot() ("https://juce.backend/") as a real
            // network request and show a browser error page. JUCE only builds
            // the WebView2 platform part when the backend is exactly
            // Backend::webview2 (see juce_WebBrowserComponent_windows.cpp), so
            // this must be requested explicitly. Inert on macOS/Linux, where
            // the platform part ignores the backend.
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            // The default WebView2 user data folder may be denied access when
            // the host DAW lives under a protected path, which silently falls
            // back to IE as well. Point it at a writable per-plugin folder
            // instead (see WinWebView2::withUserDataFolder and
            // WebViewPluginDemo.h in the JUCE examples).
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (juce::File::getSpecialLocation (
                                             juce::File::SpecialLocationType::tempDirectory)
                                             .getChildFile ("CacophonicClip-WebView2")))
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
