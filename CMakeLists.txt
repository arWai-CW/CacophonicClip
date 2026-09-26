cmake_minimum_required(VERSION 3.22)

# clang-tidy (uvx clang-tidy) 讀 build/compile_commands.json，缺了就 lint 不到
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Bundle 版本：沒指定就用最近的 git tag，讓 Finder「取得資訊」顯示的版本跟著 release 走，
# 不要在這裡寫死，寫死就會出現 release 1.0.0 但 Finder 顯示 0.1.0 的情況。
# CI 會在 tag build 用 -DCLIP_VERSION=<tag> 指定，PR build 傳 0.0.0。
set(CLIP_VERSION "" CACHE STRING "Bundle version, defaults to the most recent git tag")

if(NOT CLIP_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            OUTPUT_VARIABLE CLIP_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
endif()

# 容忍 v1.2.3 這種 tag 前綴
string(REGEX REPLACE "^v" "" CLIP_VERSION "${CLIP_VERSION}")

# _juce_version_code 會把各段餵給 math(EXPR)，非數字會讓 configure 直接爆掉，所以這裡擋掉
if(NOT CLIP_VERSION MATCHES "^[0-9]+(\\.[0-9]+)*$")
    set(CLIP_VERSION "0.0.0")
endif()

# 清理完才初始化 build version，不然 v 前綴或被擋掉的非法值會漏進 CFBundleVersion
set(CLIP_BUILD_VERSION "${CLIP_VERSION}")

# 不在任何 tag 上時，把距離最近 tag 幾個 commit 補進 build version，
# 這樣自建版至少看得出跟正式版不是同一個
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-list --count "${CLIP_VERSION}..HEAD"
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        OUTPUT_VARIABLE _clip_commits_ahead
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_clip_commits_ahead MATCHES "^[1-9][0-9]*$")
        set(CLIP_BUILD_VERSION "${CLIP_VERSION}.${_clip_commits_ahead}")
    endif()
endif()

message(STATUS "Cacophonic Clip version: ${CLIP_VERSION} (build ${CLIP_BUILD_VERSION})")

project(CacophonicClip VERSION ${CLIP_VERSION})

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
    VERSION ${CLIP_VERSION}
    BUILD_VERSION ${CLIP_BUILD_VERSION}
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
