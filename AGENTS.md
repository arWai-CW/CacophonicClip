# AGENTS.md

VST3 clipper「Cacophonic Clip」= C++/JUCE 外掛（repo 根目錄）＋ SvelteKit WebView UI（`ui/`）。唯一設計依據是 **`docs/DESIGN_SPEC.md`**（含 §11.9 / §12.8 驗收清單）；實作與文件衝突時以程式碼與設定為準。

## 結構

- 根目錄：CMake + JUCE 插件，`Source/PluginProcessor.*`、`Source/PluginEditor.*`、`Source/dsp/ClipperDSP.*`。JUCE 是 **git submodule**（`.gitmodules`），新環境要 `git submodule update --init`。
- `ui/`：SvelteKit + Svelte 5 + Tailwind 4，套件管理用 **bun**（不是 npm/pnpm）。`ui/AGENTS.md` 有 Svelte MCP 的補充指示（僅在 `ui/` 內工作時適用）。
- `Source/ui_dist/` 與 `Source/UIResources.zip` 都是 **gitignore 的產生檔**；`WebResourceProvider.h` 讀的是 zip。`ui/build/` 是舊 UI 殘留，勿當成輸出。
- `tools/macos-installer/`：macOS 分發用的安裝包（見下方「macOS 分發」）。
- CI 在 `.github/workflows/build.yml`（GitHub Actions）。無 C++ 測試；測試只在 `ui/`（`bun:test`，如 `ui/src/lib/uiState.test.ts`）。C++ 品質靠 lint（見下方「C++ lint」），check 集合寫在根目錄 `.clang-tidy`。

## 命令

UI 驗證（順序固定，全部在 `ui/`）：

```bash
cd ui && bun run lint && bun run check && bun run test && bun run build
```

交付（`UIResources.zip` **不會自動重建**，兩步都要做）：

```bash
(cd Source/ui_dist && zip -ry ../UIResources.zip . -x '.DS_Store')
cmake --build build -j        # build/ 已是 Debug 設定，COPY_PLUGIN_AFTER_BUILD 會安裝 VST3
```

C++ 改動只需 `cmake --build build -j`（0 error 0 warning 是驗收標準）。若 `build/` 不存在：`cmake -B build`。改完 `Source/` 下的 .cpp/.h 之後接著跑下面的 C++ lint，兩者都要過。

## C++ lint（改完 C++ 檔案必跑）

兩個工具都用 `uvx` 現抓，不需預先安裝。前置：`build/compile_commands.json` 要存在（`CMakeLists.txt` 已設 `CMAKE_EXPORT_COMPILE_COMMANDS ON`；新環境或 build/ 不在就先 `cmake -B build`）。

```bash
# 1) cppcheck：故意不給 JUCE include，理由見下面坑 1
uvx cppcheck --enable=warning,performance,portability,style --std=c++17 --language=c++ \
  --check-level=exhaustive --inline-suppr --error-exitcode=1 Source

# 2) clang-tidy：讀 build/compile_commands.json；-isysroot 不可省
uvx clang-tidy -p build --quiet --warnings-as-errors='*' \
  --extra-arg=-isysroot$(xcrun --show-sdk-path) \
  Source/*.cpp Source/dsp/*.cpp
```

- 驗收：兩條都 exit 0 且零 finding。`--error-exitcode=1`（cppcheck）與 `--warnings-as-errors='*'`（clang-tidy）就是 gate，有 finding 會升級成 error 回非 0。
- 新增 .cpp 放在 `Source/` 或 `Source/dsp/`，上面的 glob 會自動涵蓋；clang-tidy 只認清單裡的檔案，記得確認 glob 有吃到。
- `.clang-tidy` 的 `HeaderFilterRegex` 只對 `Source/` 出報告（JUCE 與 `Source/ui_dist/` 不算我們的程式碼）。改設定後用 `uvx clang-tidy --verify-config` 驗一次；`#` 註解只能寫在 `Checks: >` block 的外面，寫進去會被當成 glob 的一部分。
- 臨時壓掉單條 finding：clang-tidy 寫 `// NOLINT(<check-id>)`，cppcheck 寫 `// cppcheck-suppress <check-id>`（所以 cppcheck 帶 `--inline-suppr`）。能修就修，壓制要有理由。
- 坑 1：**不要**改用 `uvx cppcheck --project=build/compile_commands.json`。JUCE header 會噴 `#error "Unknown platform!"` 之類，cppcheck 進 critical error 狀態後 checkers 從 166/856 掉到 4/856，等於沒在檢查卻還回 0。
- 坑 2：clang-tidy 少了 `-isysroot` 會噴 `TargetConditionals.h file not found`（uvx 的 LLVM 不自帶 Apple SDK 路徑），`Source/*.cpp` 全部分析失敗。
- 坑 3：`.clang-tidy` 已停用 `bugprone-pointer-arithmetic-on-polymorphic-object`，LLVM 22 的 matcher 在 JUCE `juce_Ranges.h` 上會直接 crash。
- lint 只看靜態品質，跑完仍要 `cmake --build build -j` 確認 0 error 0 warning。

## 開發流程

- `cd ui && bun run dev` → `http://127.0.0.1:5173`。`vite.config.ts` 鎖 `strictPort`，**port 不可改**：`Source/PluginEditor.cpp` 的 `getDevServerUrl()` 會 probe 這個位址。
- Debug build 下 dev server 一開就走 HMR，一關就回內嵌 zip，**C++ 不用重 build**。
- `CLIP_DEV_UI_URL=<url>` 強制指定 URL（任何 build type）；`CLIP_DEV_UI_URL=off` 強制內嵌 zip。
- `ui/src/routes/+layout.ts` 的 `ssr = false` 不可移除，否則 `@juce-framework/webview` 在 module 層讀 `window.__JUCE__` 會 SSR 掛掉。
- 純瀏覽器開 dev server 只有 mock 版面，沒有參數與 meter；要觸發 meter 用 `window.__JUCE__.backend.emitByBackend('meterData', JSON.stringify(payload))`，且注入與讀 DOM 要分不同 task（Svelte 5 DOM 更新在 microtask）。

## macOS 分發（dmg 與安裝包）

- dmg 與安裝包都由 CI 的 `package-mac` job 產（只有推 tag 才跑），本機不用手動打包。
- 來源檔在 `tools/macos-installer/`：`InstallCacophonicClip.applescript`（安裝邏輯）＋ `build-installer.sh`（`osacompile` 成 `.app`、把外掛塞進 `Contents/Resources`、ad-hoc 簽章）。`docs/INSTALL-macOS.txt` 會被放進 dmg 根目錄。
- **簽章現況只有 ad-hoc**：沒有 Apple Developer Program，所以沒有 Developer ID 也沒有公證。arm64 slice 一定要有簽章才載得進去（JUCE 自動加 `-adhoc_codesign`），**絕對不能 `--remove-signature`**。實測 ad-hoc 在 macOS 27 上 Logic 正常載入，沒有擋。
- 為什麼要包成 `.app`：未簽章的 `.vst3` / `.component` 被 Gatekeeper 擋時，Finder 右鍵選單**不會**出現「打開」，只有「完成／移到垃圾桶」；`.command` 腳本一樣被擋（`open` 完全沒反應）。`.app` 是唯一吃得到「右鍵 → 打開」的容器。**不要改回 `.pkg`**：實測未簽章的 pkg 在 macOS 27 直接被 Installer 判定「與您的 Mac 版本不相容」而中止。
- dmg 裡只放 `.app` 與 `INSTALL.txt`，**不要**把原始 `.vst3` / `.component` 一起放，那對使用者是陷阱。
- 安裝包只寫 `~/Library/Audio/Plug-Ins/`（per-user），不碰 `/Library`、不需要管理員密碼。曾經做過「安裝給所有使用者」的選項，實測 `with administrator privileges` 的授權流程走不完，已拿掉，別加回來。
- `ditto` **會保留**來源的 quarantine 屬性（`--noqtn` 與 `COPYFILE_DISABLE=1` 都擋不掉），所以安裝完必須顯式 `xattr -cr`，不然 Gatekeeper 照樣擋。
- 改完安裝包要本機驗一次：`tools/macos-installer/build-installer.sh build/CacophonicClip_artefacts/Debug "<輸出>.app"`，再 `codesign --verify --strict`。
- **不做 AAX**：Windows 上沒有程式碼簽章，未簽章的 AAX bundle 使用者裝不乾淨。要加回來得先解決 Windows 程式碼簽章，否則 `CLIP_FORMATS` 保持只有 `VST3`（macOS 再加 `AU`）。
- dmg 用 `diskutil image create from`，不要用 `hdiutil create -volname`（macOS 27 已 deprecated）；校驗用 `hdiutil verify`。
- 使用者可見的字串（`docs/INSTALL-macOS.txt`、安裝包對話框、dmg 與 `.app` 的檔名）一律英文，面向廣泛的網路使用者；`Source/` 與 `tools/` 的程式註解也用英文。build/CI 層的註解維持中文。
- 拿到 Apple Developer Program 之後：Developer ID 簽章與 `xcrun notarytool submit --wait` 加在 `package-mac` job，**不要**塞進 `build` job（那個也服務 PR，不能碰憑證）。公證要 `--options=runtime` hardened runtime，而本外掛的 UI 是 WKWebView，可能需要補 `com.apple.security.cs.allow-jit` entitlement，簽完要實機確認 UI 能不能起來。

## 硬性約束（DESIGN_SPEC §2 §9，違反即驗收失敗）

- 禁止色：綠 `#a9c891`、青 `#8fd2e8`、純黑 `#000000`、純白 `#ffffff`。單一琥珀 accent，紅 `--over` 只代表「正在被切／過載」。
- 文案：零 em-dash / en-dash（日期用連字號）、零中點 `·`。禁止 `Inter`、禁止 Google Fonts `<link>`（字體 self-host woff2）。
- 全域 `radius: 0`；單一 locked 暗主題，CSS 無 `dark:`；無 scroll／入場動畫。
- `.panel` 上 **禁止 `backdrop-filter`**（panel 有 `transform: scale()`，是 backdrop root，會掉幀閃一下）。
- 全域禁止文字選取（面板是 control surface）。

## 程式慣例

- **參數一律追加到 `createParameterLayout` 最尾端**，維持既有索引穩定。
- 旋鈕 arc **只能用 `arcPath()` 顯式 `A` command**；`.knob-arc` / `.knob-groove` 禁止 `stroke-dasharray` + `pathLength`（Chrome 在 screen space 量 dash，掃角會錯，見 §11.0 決定 34）。
- `SHAPE` 等讀數格式必須與 `PluginProcessor.cpp` 的 `StringFromValue` 完全一致。
- `getMimeType()` 必須支援 `jpg` 與 `webp`，否則 WebKit 拒載圖片。
- 本 JUCE 無 `setBypassParameter`：覆寫 virtual `getBypassParameter()`。
- 新增 relay／attachment 要比照既有 `WebSliderRelay` + `withOptionsFrom(...)` + attachment 的成對寫法。
