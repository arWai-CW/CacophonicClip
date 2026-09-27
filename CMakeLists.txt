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

# 一定要顯式鎖 macOS deployment target，理由是實測踩過：
# 不設的話 CMake 會留空，clang 退回自己的預設值，minos 直接等於 build 主機的 SDK 版本。
# 本機是 macOS 27 SDK 就是 minos 27.0，CI 的 macos-latest 至少是 15 起跳，
# 結果 plugin 在舊一點的 macOS 上連載都載不進去，Logic 的外掛清單裡根本不會出現。
# 11.1 是 UI 的硬下界，不是隨便挑的數字：layout.css 用了 8 處 `inset:` 與 6 處
# flexbox `gap:`，兩者都要 Safari 14.1（= macOS 11.1），而 .stage / .texture /
# CRT overlay 的定位與 .ladder 的分段都靠它們。實測機是 11.7.6 / Logic 10.4.7。
# 不要再往下調：SDK 已經不保證更舊的版本，而低於 10.13 的 target 會開始噴一堆
# deprecation warning，違反 0 warning 的驗收標準。
# 用普通變數而不是 CACHE 變數：已經存在的 build 目錄，cache 裡通常有一個空的
# CMAKE_OSX_DEPLOYMENT_TARGET entry，那種情況下 set(... CACHE ...) 會 no-op，
# 等於什麼都沒做。普通變數會 shadow 掉 cache 值，所以一定生效。
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.1")
endif()

# CI 用 -DCLIP_COPY_PLUGIN_AFTER_BUILD=OFF 關閉（runner 沒有系統插件目錄需求）
set(CLIP_COPY_PLUGIN_AFTER_BUILD ON CACHE BOOL "Copy the built plugin into the system plugin folders")

add_subdirectory(JUCE)

# 不做 AAX：Windows 上沒有程式碼簽章，未簽章的 AAX bundle 使用者裝不乾淨
# （AAX SDK 對 bundle 內容有自己的驗證）。要做 AAX 得先解決 Windows 簽章。
set(CLIP_FORMATS VST3)
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

# JUCE 的 ad-hoc 簽章有兩條路徑：VST3 target 建立時會簽一次，但把「所有非 App 的 target
# 都補簽一次」的那條掛在 juce_enable_copy_plugin_step 裡，而那個 function 只在
# COPY_PLUGIN_AFTER_BUILD 開啟時才會被呼叫。CI 傳 CLIP_COPY_PLUGIN_AFTER_BUILD=OFF，
# 結果留下兩個問題：AU 完全沒有 bundle 簽章；VST3 的 seal 在 moduleinfo.json 被加進
# bundle 之後就過期了（codesign --verify --strict 報 sealed resource is missing or
# invalid）。所以自己在 build 之後補一次，兩種 COPY 設定的產物都一致。
#
# add_custom_command 的 POST_BUILD 依加入順序執行，而 juce_add_plugin 在上面，
# 所以這裡一定是最後執行，seal 才會包含 moduleinfo.json。
# 拿到 Developer ID 之後，把 --sign - 換成正式憑證並補上公證。
if(APPLE)
    foreach(format IN LISTS CLIP_FORMATS)
        add_custom_command(TARGET "CacophonicClip_${format}" POST_BUILD
            COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:CacophonicClip_${format}>"
            VERBATIM)
    endforeach()
endif()
