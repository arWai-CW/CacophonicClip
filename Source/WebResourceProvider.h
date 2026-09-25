#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "BinaryData.h"

#include <juce_core/juce_core.h>

class WebResourceProvider
{
public:
    static std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url)
    {
        const auto path = normalisePath (url);
        auto* archive = getArchive();

        if (archive == nullptr)
            return std::nullopt;

        if (const auto* entry = archive->getEntry (path))
        {
            auto stream = rawToUniquePtr (archive->createStreamForEntry (*entry));
            const auto size = static_cast<size_t> (stream->getTotalLength());
            std::vector<std::byte> data (size);
            stream->read (data.data(), size);

            return juce::WebBrowserComponent::Resource {
                std::move (data),
                getMimeType (path)
            };
        }

        return std::nullopt;
    }

private:
    static juce::ZipFile* getArchive()
    {
        static juce::MemoryInputStream stream { BinaryData::UIResources_zip,
                                                  BinaryData::UIResources_zipSize,
                                                  false };
        static juce::ZipFile archive { stream };
        return &archive;
    }

    static juce::String normalisePath (const juce::String& url)
    {
        auto path = url;

        if (path.startsWith ("/"))
            path = path.fromFirstOccurrenceOf ("/", false, false);

        if (path.isEmpty())
            path = "index.html";

        return path;
    }

    static juce::String getMimeType (const juce::String& path)
    {
        const auto extension = path.fromLastOccurrenceOf (".", false, false).toLowerCase();

        if (extension == "html") return "text/html";
        if (extension == "js") return "text/javascript";
        if (extension == "css") return "text/css";
        if (extension == "json") return "application/json";
        if (extension == "svg") return "image/svg+xml";
        if (extension == "png") return "image/png";
        if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
        if (extension == "webp") return "image/webp";
        if (extension == "ico") return "image/x-icon";
        if (extension == "woff2") return "font/woff2";

        return "application/octet-stream";
    }
};
