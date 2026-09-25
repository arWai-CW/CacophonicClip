cmake_minimum_required(VERSION 3.22)

# clang-tidy (uvx clang-tidy) 讀 build/compile_commands.json，缺了就 lint 不到
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(CacophonicClip VERSION 0.1.0)

add_subdirectory(JUCE)

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
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 Standalone
    NEEDS_WEB_BROWSER TRUE
    NEEDS_CURL FALSE
)

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
