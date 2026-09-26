cmake_minimum_required(VERSION 3.22)

# clang-tidy (uvx clang-tidy) 讀 build/compile_commands.json，缺了就 lint 不到
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(CacophonicClip VERSION 0.1.0)

# CI 用 -DCLIP_COPY_PLUGIN_AFTER_BUILD=OFF 關閉（runner 沒有系統插件目錄需求）
set(CLIP_COPY_PLUGIN_AFTER_BUILD ON CACHE BOOL "Copy the built plugin into the system plugin folders")

add_subdirectory(JUCE)

# AAX SDK 內建在 JUCE submodule（juce_audio_plugin_client/AAX/SDK），不需另外安裝
set(CLIP_FORMATS VST3 AAX)
if(APPLE)
    list(APPEND CLIP_FORMATS AU) # AU 只有 macOS 有
endif()

juce_add_plugin(CacophonicClip
    COMPANY_NAME "CacophonicTones"
    BUNDLE_ID "com.cacophonictones.clip"
    PLUGIN_MANUFACTURER_CODE Caco
    PLUGIN_CODE Clp1
    PRODUCT_NAME "Cacophonic Clip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${CLIP_COPY_PLUGIN_AFTER_BUILD}
    FORMATS ${CLIP_FORMATS}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_CURL FALSE
    NEEDS_WEBVIEW2 TRUE
)

# Windows 必須走 WebView2 才有 resource provider（PluginEditor.h 的 withResourceProvider
# 被 JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE 擋掉時會編譯失敗）。
# 靜態連結免帶 WebView2Loader.dll；需要 NuGet 套件 Microsoft.Web.WebView2，
# CI 用 -DJUCE_WEBVIEW2_PACKAGE_LOCATION 指到下載位置，本機則裝到 NuGet 預設目錄。
if(WIN32)
    target_compile_definitions(CacophonicClip PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

juce_add_binary_data(UIResources
    SOURCES
        Source/UIResources.zip
)

target_link_libraries(CacophonicClip PRIVATE UIResources)

target_sources(CacophonicClip
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/ClipperDSP.cpp
)

target_link_libraries(CacophonicClip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(CacophonicClip
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
)
