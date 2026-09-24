#include "PluginEditor.h"

namespace
{
// docs/DESIGN_SPEC.md: the panel is a fixed 1120x560 CSS-px canvas drawn at
// transform: scale(k), where k is clamped to [0.8, 1.5] on the web side. The
// editor window is the only thing that decides k (k = min(w/1120, h/560)), so
// it has to open at exactly the design size and every resize has to stay on
// that same 2:1 ratio inside the 896x448 .. 1680x840 band. Any other window
// shape letterboxes the canvas and leaves dead bands around the UI.
constexpr int designWidth = 1120;
constexpr int designHeight = 560;
constexpr int minWidth = designWidth * 4 / 5;   // k = 0.8  -> 896
constexpr int minHeight = designHeight * 4 / 5; //            448
constexpr int maxWidth = designWidth * 3 / 2;   // k = 1.5  -> 1680
constexpr int maxHeight = designHeight * 3 / 2; //            840
constexpr double designRatio = designWidth / static_cast<double>(designHeight);

// Window-size memory. The editor opens at the design size by default, but a
// size the user dragged the window to should survive closing and reopening
// the plugin. The file lives in ~/Library/Application Support/CacophonicTones/
// and is shared by every instance (VST3, standalone, whatever the host is).
// The interprocess lock stops two instances closing at once from interleaving
// their writes; member order matters - `file` must die before `lock`.
struct SizeStore
{
    SizeStore()
        : lock("CacophonicClipEditorSize")
    {
        options.applicationName = "CacophonicClip";
        options.folderName = "CacophonicTones";
        options.osxLibrarySubFolder = "Application Support";
        options.filenameSuffix = ".settings";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        // No timer: the editor writes on destruction and ~PropertiesFile
        // flushes anything that is still pending.
        options.millisecondsBeforeSaving = -1;
        options.processLock = &lock;
        file = std::make_unique<juce::PropertiesFile>(options);
    }

    juce::InterProcessLock lock;
    juce::PropertiesFile::Options options;
    std::unique_ptr<juce::PropertiesFile> file;
};

// Which HTML the WebView loads. The embedded zip served through the resource
// provider is the default and stays the only choice for anything shipped.
// While the UI is being developed, `bun run dev` in ui/ runs Vite with hot
// reload, and pointing the WebView at it removes the build -> zip -> rebuild
// cycle entirely. JUCE injects the native integration (slider/toggle/combo
// relays, meter events) as a user script at document start, so it works for
// the dev server URL exactly as it does for the embedded page - parameters
// and meters keep running, only the HTML comes from Vite.
//
//   CLIP_DEV_UI_URL=<url>   load this URL (any build type; useful when a
//                           Release plugin still wants the dev server)
//   CLIP_DEV_UI_URL=off     always use the embedded zip, never probe
//   unset, JUCE_DEBUG build probe 127.0.0.1:5173 and use it if it answers
//   unset, release build    embedded zip
juce::String getDevServerUrl()
{
    const auto overrideUrl =
        juce::SystemStats::getEnvironmentVariable("CLIP_DEV_UI_URL", {});

    if (overrideUrl.isNotEmpty())
        return overrideUrl.equalsIgnoreCase("off") ? juce::String() : overrideUrl;

#if JUCE_DEBUG
    // A closed port refuses the connection immediately, so this only costs
    // time when something actually holds 5173. The host must match the
    // `server.host` pinned in ui/vite.config.ts.
    constexpr int devServerPort = 5173;

    juce::StreamingSocket probe;
    if (probe.connect("127.0.0.1", devServerPort, 200))
    {
        probe.close();
        return "http://127.0.0.1:" + juce::String(devServerPort) + "/";
    }
#endif

    return {};
}
} // namespace

CacophonicClipEditor::CacophonicClipEditor(CacophonicClipProcessor &processorToUse)
    : AudioProcessorEditor(&processorToUse),
      processorRef(processorToUse),
      webTrimAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::trim), trimRelay)),
      webDriveAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::drive), driveRelay)),
      webMixAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::mix), mixRelay)),
      webShapeAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::shape), shapeRelay)),
      webEmphasisAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::emphasis), emphasisRelay)),
      webAsymAttachment(std::make_unique<juce::WebSliderParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::asym), asymRelay)),
      webBoostAttachment(std::make_unique<juce::WebToggleButtonParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::boost), boostRelay)),
      webBypassAttachment(std::make_unique<juce::WebToggleButtonParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::bypass), bypassRelay)),
      webOversamplingAttachment(std::make_unique<juce::WebComboBoxParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::oversampling), oversamplingRelay)),
      webEmphasisModeAttachment(std::make_unique<juce::WebComboBoxParameterAttachment>(
          *processorRef.getParameters().getParameter(ParameterIDs::emphasisMode), emphasisModeRelay))
{
    addAndMakeVisible(webView);

    // Dev server if one is running, the embedded zip otherwise. Only this one
    // call decides it; nothing else in the editor cares where the HTML came
    // from, so opening the editor again picks up whichever source exists now.
    if (const auto devServerUrl = getDevServerUrl(); devServerUrl.isNotEmpty())
    {
        DBG("CacophonicClip: loading UI from dev server " + devServerUrl);
        webView.goToURL(devServerUrl);
    }
    else
    {
        webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    }

    startTimerHz(60);

    // Open at the design size so the canvas starts at scale 1 with no bands.
    setSize(designWidth, designHeight);

    // Host and corner resizes both run through the constrainer: the design
    // ratio is pinned and the size is clamped to the 0.8 .. 1.5 scale band.
    setResizable(true, true);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    if (auto *constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(designRatio);

    // ...unless the user left the window at another size last time. A size
    // stored by an older build can be off-ratio or out of band, so it always
    // goes through the constrainer below before it reaches the screen.
    {
        SizeStore store;
        const auto storedWidth = store.file->getIntValue("editorWidth");
        const auto storedHeight = store.file->getIntValue("editorHeight");
        if (storedWidth > 0 && storedHeight > 0)
            setSize(storedWidth, storedHeight);
    }

    setBoundsConstrained(getBounds());
}

CacophonicClipEditor::~CacophonicClipEditor()
{
    // Remember the size the window was closed at. Skip a zeroed editor: some
    // hosts tear the component down to 0x0 before calling the destructor, and
    // storing that would resurrect a window no one can see.
    if (getWidth() > 0 && getHeight() > 0)
    {
        SizeStore store;
        store.file->setValue("editorWidth", getWidth());
        store.file->setValue("editorHeight", getHeight());
        // ~PropertiesFile flushes the pending write to disk.
    }
}

void CacophonicClipEditor::paint(juce::Graphics &graphics)
{
    graphics.fillAll(juce::Colour(0xff101216));
}

void CacophonicClipEditor::resized()
{
    // Some hosts hand the editor a size without going through
    // checkSizeConstraint() - a size remembered from an older build, say.
    // Push those through the same constrainer so the hard-coded ratio and the
    // scale band hold no matter who asked for the resize. If the bounds are
    // already legal this is a no-op; the flag stops the constrainer's own
    // setBounds() from looping back in here.
    if (!applyingBoundsConstraint && getConstrainer() != nullptr)
    {
        const juce::ScopedValueSetter<bool> scope(applyingBoundsConstraint, true);
        setBoundsConstrained(getBounds());
    }

    webView.setBounds(getLocalBounds());
}

void CacophonicClipEditor::timerCallback()
{
    const auto snapshot = processorRef.getMeterSnapshot();
    auto* payload = new juce::DynamicObject();
    payload->setProperty("inputLeft", snapshot.inputLeft);
    payload->setProperty("inputRight", snapshot.inputRight);
    payload->setProperty("outputLeft", snapshot.outputLeft);
    payload->setProperty("outputRight", snapshot.outputRight);
    // Ring buffer: writeIndex points to the next slot to write, so the oldest
    // sample starts at writeIndex when the buffer is full. Reorder to
    // chronological order (oldest -> newest) before sending to the UI.
    const auto pointCount = MeterSnapshot::waveformPointCount;
    const auto writeIndex = snapshot.waveformWriteIndex;
    const auto filled = juce::jlimit(0, pointCount, snapshot.waveformFilled);
    auto appendChronological = [writeIndex, filled]
        (const std::array<float, MeterSnapshot::waveformPointCount>& source, juce::Array<juce::var>& destination)
    {
        const auto start = filled == pointCount ? writeIndex : 0;
        for (int i = 0; i < filled; ++i)
            destination.add(source[static_cast<size_t>((start + i) % pointCount)]);
    };
    juce::Array<juce::var> inputWaveformMin;
    juce::Array<juce::var> inputWaveformMax;
    juce::Array<juce::var> drivenWaveformMin;
    juce::Array<juce::var> drivenWaveformMax;
    juce::Array<juce::var> outputWaveformMin;
    juce::Array<juce::var> outputWaveformMax;
    appendChronological(snapshot.inputWaveformMin, inputWaveformMin);
    appendChronological(snapshot.inputWaveformMax, inputWaveformMax);
    appendChronological(snapshot.drivenWaveformMin, drivenWaveformMin);
    appendChronological(snapshot.drivenWaveformMax, drivenWaveformMax);
    appendChronological(snapshot.outputWaveformMin, outputWaveformMin);
    appendChronological(snapshot.outputWaveformMax, outputWaveformMax);
    payload->setProperty("inputWaveformMin", inputWaveformMin);
    payload->setProperty("inputWaveformMax", inputWaveformMax);
    payload->setProperty("drivenWaveformMin", drivenWaveformMin);
    payload->setProperty("drivenWaveformMax", drivenWaveformMax);
    payload->setProperty("outputWaveformMin", outputWaveformMin);
    payload->setProperty("outputWaveformMax", outputWaveformMax);
    webView.emitEventIfBrowserIsVisible("meterData", juce::var(payload));
}
