# Cacophonic Clip — 設計規格（定案）

> 本文件只保留計畫階段的**決定**，不含討論過程。實作以本文件為唯一依據。

## 0. 定位

VST3 clipper 的儀器面板（非行銷頁）。對象：混音／母帶工程師。
參照系：**70–80 年代機架式處理器**（UREI 1176 / Empirical Labs Distressor 一路）。

Dials：`DESIGN_VARIANCE: 4`、`MOTION_INTENSITY: 2`、`VISUAL_DENSITY: 5`。

## 1. 核心決定（28 題結案）

| # | 決定 |
|---|---|
| 1 | 參照系＝70–80s 機架式處理器面板 |
| 2 | 視覺 overhaul ＋ 版面自由重排；**全部功能語意保留**（Trim/Drive/Mix/Shape/2x、雙聲道 meter、waveform、transfer curve） |
| 3 | 文案砍到只剩面板絲印 |
| 4 | 固定畫布 `1120×560`，按比例縮放 |
| 5 | 打包 woff2 字體 ＋ 紋理全用 SVG/CSS 程序生成（plugin 必須離線） |
| 6 | `MOTION_INTENSITY: 2`：無入場／捲動動畫，只有 meter ballistics、旋鈕回饋、開關按壓、OVER 燈 |
| 7 | 面板＝深灰鎂色 ＋ 象牙絲印 ＋ 單一琥珀 accent ＋ 紅色僅語意；**單一 locked 暗主題**，無 light mode、無切換、無 section 翻轉 |
| 8 | Meter＝工業式 LED 階梯，單色琥珀、紅頂 2 段；**綠色移除** |
| 9 | `Shape`＝連續旋鈕 `0–100%`，`0=SOFT (tanh)`、`100=HARD`，**必須連動 transfer curve 的 knee 與 ceiling** |
| 10 | **Transfer curve 當主角**，waveform 降為下方窄帶 |
| 11 | 字體：`Archivo Narrow`（絲印）＋ `IBM Plex Mono`（讀數）＋ `Archivo`（品牌），全部 self-host woff2 |
| 12 | 構圖＝上下兩帶：顯示窗在上、控制列在下，**整塊一張連續面板**，控制項之間無個別邊框 |
| 13 | Drive 放大，其餘四個等大、共用同一條基線 |
| 14 | 材質：**四角螺絲 ＋ 最多兩條 engraved seam ＋ 極淡拉絲紋理**。無銘牌、無散熱孔、無旋鈕凹座 |
| 15 | **紅色＝「訊號正在被切／過載」一件事**：只准用在 OVER 燈、waveform 的 CLIPPED 區、meter 最頂 2 段。2x 踏下用**琥珀亮起 ＋ 按鈕下沉** |
| 16 | 控制列排序（重要性優先，非訊號流程）：**`DRIVE → 2x → MIX → SHAPE → TRIM`** |
| 17 | LED ladder 嵌在**同一個顯示窗 bezel 內**左右；`OVER` 燈放顯示窗右上角、bezel 內側 |
| 18 | 讀數移到旋鈕**正下方**固定行，旋鈕面只剩 pointer ＋ 刻度環；讀數用 `IBM Plex Mono` |
| 19 | 顯示窗：**貫穿全寬的水平 ceiling 照行 ＋ 動態 dBFS 標籤** ＋ 最簡十字 graticule（只留中心十字與中線），無方格網 |
| 20 | 2x 標籤＝`2x` ＋ 小字 `INPUT GAIN` |
| 21 | 絲印：品牌行保留、**`JOY STURGIS TONES` 移除**、`IN/OUT/CLIPPED` 圖例移除、**TRIM 絲印降一級（小一號 ＋ 象牙降到灰）**、**不加銘牌／型號** |
| 22 | **Trim 改為後級輸出 trim**：`out = (input + mix×(clipped−input)) × trimGain`，跟 Drive 不再是同一條軸 |
| 23 | 加 **BYPASS** 搖臂開關，放面板**右上角**，琥珀 lamp，標籤寫 `BYPASS`（不寫 `POWER`） |
| 24 | BYPASS 的 plumbing（參數＋relay＋attachment）**與 UI 同輪交付**；「引擎」限定為 `ClipperDSP` 的 trim 搬移 |
| 25 | UI **一次到位對準目標語意**：絲印主標 `TRIM` ＋ 副標 `OUTPUT`、curve 把 trim 當輸出垂直縮放、ceiling 線隨之下移、標籤動態 |
| 26 | `OVER` 燈語意＝**「正在被切」**：`driven` 峰值穿越 clip ceiling `±1.0` 即亮（3 幀確認、`>1.002` 才觸發、`≤1.0` 才釋放、200ms 釋放尾），帶遲滯避免閃爍。**不可**拿 `driven` 對 `output` 比：trim 已改後級，`output` 永遠低於 ceiling，燈會是死亮。 |
| 27 | 圓角系統＝**全直角 `radius: 0`**，唯一曲線是圓形旋鈕與圓形燈 |
| 28 | 縮放 `k ∈ [0.8, 1.5]`（`896×448` – `1680×840`），畫布釘 `1120×560` CSS px ＋ `transform: scale(k)` |

### 實作期假設（已核准）

1. 最小字級地板：`sub-label` 9px、TRIM 主標 10px、window micro 9px；`k=0.8` 時最小 7.2px。若吃力，`k_min` 提到 `0.9`。
2. 轉移曲線**不含** mix：curve = clip 階段 ＋ output trim。
3. 旋鈕外圈加 270° 弧的 9 條 1px 放射刻度（`#A7ABA4`），可整組移除。

## 2. 色票 Token

| Token | 值 | 用途 |
|---|---|---|
| `--panel` | `#33373A` | 面板底（鎂灰） |
| `--panel-hi` | `#43474A` | engraved seam 下緣高光 |
| `--panel-lo` | `#26292B` | engraved seam、旋鈕刻度溝 |
| `--window` | `#16181A` | 顯示窗底（凹陷玻璃） |
| `--bezel-out` | `#15171A` | 顯示窗外框 |
| `--bezel-in` | `#4A4E51` | 顯示窗內緣高光 |
| `--ivory` | `#E8E3D6` | 主絲印、旋鈕 pointer（panel 上 9.4:1） |
| `--dim` | `#A7ABA4` | 次級絲印、TRIM 降級標、graticule（panel 上 5.15:1） |
| `--amber` | `#E0A33C` | **唯一 accent**：讀數、value arc、curve、meter 琥珀段、lamp（panel 5.4:1 / window 8.1:1） |
| `--over` | `#F2543C` | **僅語意**：OVER 燈、CLIPPED 區、meter 頂 2 段 |
| `--knob-cap` | `#2A2D2F` | 旋鈕帽 |
| `--slot-off` | `#1B1E20` | LED 未亮段（比窗底略亮即可，過亮會搶走 curve 主角地位） |
| `--lamp-off` | `#4A3A1C` | 熄燈的 lamp（暗琥珀，讓它讀作「未亮的燈」而非一個洞） |

**禁止**：綠色 `#a9c891`、青色 `#8fd2e8`（現況的第三、第四個顏色，全數移除）。
**禁止**：純黑 `#000000`、純白 `#ffffff`。

## 3. 字體 Token

| 角色 | 字體 | 尺寸 | 字距 | 顏色 |
|---|---|---|---|---|
| 品牌 `CACOPHONIC CLIP` | Archivo 700 | 16px | 0.14em | `--ivory` |
| 控制項主標 | Archivo Narrow 700 | 12px | 0.18em | `--ivory` |
| 控制項主標（TRIM 降級） | Archivo Narrow 700 | 10px | 0.18em | `--dim` |
| 控制項副標 | Archivo Narrow 500 | 9px | 0.14em | `--dim` |
| 讀數 | IBM Plex Mono 600 | 11px（DRIVE 12px） | 0.02em | `--amber`，`tabular-nums` |
| 顯示窗微標 | IBM Plex Mono 500 | 9px | 0.1em | `--dim`（ceiling 標籤用 `--amber`） |
| 開關標籤 | Archivo Narrow 700 | 9px | 0.16em | `--ivory` |

全部大寫、`font-display: swap`、self-host woff2（OFL）。禁止 `Inter`、禁止 Google Fonts `<link>`。

## 4. 版面座標（1120 × 560，原點左上）

內容左右邊界 `x = 32 … 1088`（寬 1056）。四角螺絲中心 `(14,14) (1106,14) (14,546) (1106,546)`，Ø10px 十字槽，四顆旋轉角各不同。

| 區域 | y 範圍 | 說明 |
|---|---|---|
| 上緣絲印條 | `0 – 50` | 左：品牌 lockup（`x=32`）。右（對齊 `x=1088`）：`[lamp 6px] [間6] [BYPASS 標籤] [間8] [搖臂 44×22]` |
| **seam 1** | `y = 50` | engraved：1px `--panel-lo` ＋ 1px `--panel-hi` 下緣，**滿版** |
| 顯示窗 | `64 – 340`（276 高，`x 32–1088`） | 單一 bezel，`radius: 0` |
| **seam 2** | `y = 354` | 同上，**滿版**（全站僅此兩條 seam） |
| 控制列 | `368 – 530`（162 高） | 直接坐在面板上，無個別邊框 |
| 下留白 | `530 – 560` | 給底部螺絲 |

上下間距對稱：seam1 下緣到窗 14px、窗到 seam2 14px、seam2 到控制列 14px。

### 4.1 顯示窗內部（水平內距 10px、垂直內距 8px）

水平：`[pad 10][LED-L 18][間10][中央 980][間10][LED-R 18][pad 10]`
垂直中央欄：`[pad-top 8][微頂 14][curve 168][divider 1][wave 65][微底 12][pad-bottom 8] = 276`

- **微頂列**：僅右端 `OVER` 燈 ＋ `OVER` 標籤（lamp 暗時標籤 `--dim`，亮時 `--over`）。**左側留空**（無標題）。
- **curve**（980 × 168）：`x ∈ [-1, 1]`（輸入 0 dBFS 歸一）、`y ∈ [-1.12, 1.12]`。
  - 曲線 `--amber` 2px `vector-effect: non-scaling-stroke`
  - ceiling 照行 `--amber` 1px `stroke-dasharray: 4 4` opacity .5，位置 `y = trimGain`
  - graticule：僅中心垂直線 ＋ 水平中線，1px `--dim` opacity .35
- **wave**（980 × 65）：input 描邊 `--dim` 1px opacity .55、output 填充 `--amber` opacity .3 ＋ 描邊、clipped `--over` opacity .85。
  - 縮放：`y = 32.5 - clamp(v, -1, 1) × 21`，**不是 ×29**。±1（clip ceiling）只走到 band 內 ±21px，剩 11px 留給紅色 overshoot；用 29 時 ±1 貼邊，紅帶被壓成 3px 幾乎看不見。
  - 紅帶內界＝常數 `±1`（clip ceiling），**不是** output envelope。trim 已是後級，`output = clipped × trimGain`，拿它當內界會在 `trim < 0 dB` 時把半條波形吞進紅色。
  - 紅帶外界＝`driven`，`|driven| > 1` 才畫，超出 1 的部分用 `1 + 0.39·(1 − e^(-(x−1)))` 壓進 1.39 倍 headroom。
- **微底列**：僅右端 ceiling 標籤，動態 `${trimDb.toFixed(1)} dBFS`（如 `0.0 dBFS` / `-6.0 dBFS`），用連字號非 en/em dash。
- **LED ladder**（各 18 × 258）：24 段，間 1px。off `--slot-off`、on `--amber`、頂 2 段 `--over`。訊源＝**post-clip 的實際輸出**（`outputLeft` / `outputRight`），讓 ladder 讀「離開插件的實際電平」；「切在哪裡」交給 curve 的 ceiling 線與紅帶。

### 4.2 控制列（五欄，總寬 1056，共用基線 `y = 528`）

| 欄 | 寬 | 內容 |
|---|---|---|
| DRIVE | 236 | 旋鈕 **Ø112** |
| 2x | 176 | 搖臂 `140×56`（含 6px 琥珀 lamp） |
| MIX | 216 | 旋鈕 Ø92 |
| SHAPE | 216 | 旋鈕 Ø92 |
| TRIM | 212 | 旋鈕 Ø92 |

垂直結構（頂對齊 `y=368`，控制列總高 162）：
- 旋鈕區 `368 – 480`（112 高，較小旋鈕垂直置中）→ 旋鈕圓心 `y = 424`
- 讀數列 `486 – 500`（11px mono，DRIVE 12px）
- 主標列 `502 – 516`
- 副標列 `517 – 528`

> 控制列原規格只有 122 高（旋鈕 112 ＋ 三列文字 ≈154），**算不進去**，造成讀數壓在旋鈕上、控制列被擠到面板底緣。已把窗上移到 `64`、seam2 上移到 `354`、控制列降到 `368` 並加高到 `162`；窗與 seam1/seam2 的間距兩側對稱 14px。

> 五欄的讀數列／主標列／副標列**完全對齊**；旋鈕圓心跨欄對齊。2x 欄的讀數列留空（狀態由搖臂位置＋lamp 表達）。

**絲印與讀數**

| 控制項 | 主標 | 副標 | 讀數 |
|---|---|---|---|
| DRIVE | `DRIVE` | `CLIPPING` | `${v.toFixed(1)} dB` |
| 2x | `2x` | `INPUT GAIN` | （空） |
| MIX | `MIX` | `DRY / WET` | `${Math.round(v*100)} %` |
| SHAPE | `SHAPE` | `SOFT / HARD` | `SOFT`（≤0.02）／`HARD`（≥0.98）／`${Math.round(v*100)} %` |
| TRIM | `TRIM`（降級） | `OUTPUT` | `${v.toFixed(1)} dB` |

`SHAPE` 讀數須與 `PluginProcessor.cpp` 的 `StringFromValue` 完全一致。

### 4.3 旋鈕規格

- 刻度弧：`270°`（`-135°` … `+135°`），3px stroke，溝槽色 `--panel-lo`
- value arc：3px `--amber`，`stroke-linecap: butt`
- 外圈刻度：9 條 1px 放射線 `--dim`（假設 3，可移除）
- 帽：圓形 `fill: --knob-cap`，半徑 `0.72 × R`，1px `--bezel-in` 內緣
- pointer：2px `--ivory`，中心至 `0.85 R`
- 無中心讀數（讀數在下方固定行）

### 4.4 開關規格

**本體＝凹槽，`::before`＝凸起的帽**，靠帽的位移表達位置，不靠標籤：
- 凹槽：`background: #191B1D`、1px `--bezel-out` 外框、`inset 0 1px 3px rgba(0,0,0,.8)`
- 帽：`inset: 3px`、`linear-gradient(180deg, #4A4F52, #34383A 52%, #2A2D2F)`、`inset 0 1px 0 rgba(255,255,255,.16)` 頂緣高光 ＋ `0 1px 2px` 投影
- 按下／踏下（`::before`）：`translateY(2px)`、高光降為 `rgba(255,255,255,.05)`、改 `inset 0 2px 5px rgba(0,0,0,.7)`，`transition: 80ms`
- `:disabled`：帽改用降對比漸層（維持可讀但明顯無作用）
- lamp：圓形，off `--lamp-off`、on `--amber` ＋ `0 0 6px` glow，`z-index: 1` 蓋在帽上

矩形搖臂 `radius: 0`。lamp 琥珀＝活動中。

## 5. 動態（MOTION 2）

| 動畫 | 機制 | 動機 |
|---|---|---|
| meter 衰減 | attack 即時、release 指數 ~300ms | 回饋訊號強度 |
| 旋鈕 value arc | 即時（無 transition） | 回饋當前值 |
| 開關位移 | `translateY(1px)` 80ms | 回饋按壓 |
| OVER 燈 | 即時亮、200ms 釋放、帶遲滯 | 回饋正在削波 |

- **零** `window.addEventListener('scroll')`、零捲動、零入場動畫。
- `prefers-reduced-motion: reduce`：meter release 改即時階梯、開關移除 transition（meter 本身是資料，保留）。
- 紋理層一律 `position: absolute; inset: 0; pointer-events: none` 靜態層，禁止在捲動容器上做 grain。

## 6. 程序紋理

- 拉絲鋁：`repeating-linear-gradient(90deg, rgba(255,255,255,.014) 0 1px, transparent 1px 3px)` ＋ 縱向光照漸層
- Grain：SVG `feTurbulence` data-URI，opacity ≤ 3%
- engraved seam：`1px --panel-lo` ＋ `0 1px 0 --panel-hi`
- bezel：`inset 0 2px 6px rgba(0,0,0,.7)` ＋ `0 1px 0 rgba(255,255,255,.06)`
- 螺絲：`radial-gradient` 圓 ＋ 1px 十字槽，**四顆旋轉角各不同**避免機械重複

無 raster 圖、無 CDN、無 div 假截圖、無手繪裝飾 SVG、無 icon（面板零 icon 需求）。

## 7. 契約（UI ↔ Engine 交接）

### 7.1 參數

| ID | 型別 | 範圍 | 說明 | 狀態 |
|---|---|---|---|---|
| `trim` | float | `-12 … 0 dB` | **改為後級輸出 trim** | 引擎待改 |
| `drive` | float | `0 … 24 dB` (skew 3) | 不變 | 已有 |
| `mix` | float | `0 … 1` | 不變 | 已有 |
| `boost2x` | bool | — | 不變（UI 讀取名 `boost2x`） | 已有 |
| `shape` | float | `0 … 1` | soft↔hard morph | **已完整接線，僅缺 Svelte 端** |
| `bypass` | bool | — | **新增**，JUCE 無 `setBypassParameter`，改覆寫 virtual `getBypassParameter()` | 已完成 |

### 7.2 Svelte 端接法

```ts
const shapeState  = getSliderState('shape');
const bypassState = getToggleState('bypass');   // 引擎新增後才有
```
既有 `trim / drive / mix / boost2x` 接法不變。

### 7.3 引擎改動

1. `ClipperDSP::process` — `trimGain` 從 `driven` 挪到最終輸出：
   ```cpp
   // 刪除 inputGain 中的 trimGain
   const auto driven = input * doubleGain * drive;
   // ...
   samples[sample] = (input + mix * (clipped - input)) * trimGain;
   ```
2. 新增 `bypass` 參數 → `createParameterLayout`（放在 layout 尾端，維持既有參數索引穩定）＋ 覆寫 `AudioProcessor::getBypassParameter()` 回傳該參數，`processBlock` 檢查並以 **5ms 線性 crossfade** 避免 click。
   - **本 JUCE 無 `AudioProcessor::setBypassParameter`**，等價作法就是覆寫 `virtual getBypassParameter() const`（host wrapper 會自己去拿）。`fadeLength = max(1, sampleRate × 0.005)`，狀態在 `prepareToPlay` 清零。
   - bypass 時 `driven` 與 `output` waveform 都記輸入原樣；driven 的顯示增益為 `2x × Drive`（**不含 trim**），因為 trim 已不屬於 clip 階段。
3. `ClipperSettings::shape` 與 `StringFromValue` 已完成，**不動**。

### 7.4 Curve 公式（UI 端）

```
gain    = 10^(drive/20) × (boost2x ? 2 : 1)
clipped = lerp(tanh(in × gain), clamp(in × gain, -1, 1), shape)
out     = clipped × 10^(trim/20)
ceiling = 10^(trim/20)          // 對應 y 位置與標籤
```
不含 `mix`（假設 2）。

## 8. 交付流程

```bash
cd ui && bun run build && cd ..           # → Source/ui_dist
(cd Source/ui_dist && zip -ry ../UIResources.zip . -x '.DS_Store')
cmake --build build                        # COPY_PLUGIN_AFTER_BUILD
```

- **`Source/UIResources.zip` 不會自動重建**，`WebResourceProvider.h` 讀的是 zip，兩步必須都做。
- `ui/build/` 是舊 UI 殘留（`svelte.config.js` 現只輸出 `../Source/ui_dist`），建議刪除避免誤判。
- `mime` 已支援 `woff2` / `svg` / `png`（`WebResourceProvider.h`）。

### 8.1 開發流程（dev server + HMR）

```bash
cd ui && bun run dev     # vite dev, http://127.0.0.1:5173
```

- `Source/PluginEditor.cpp` 的 `getDevServerUrl()` 在開編輯器時 probe `127.0.0.1:5173`（僅 `JUCE_DEBUG` build）：有 listener 就 `goToURL("http://127.0.0.1:5173/")`，否則照舊 `getResourceProviderRoot()`。所以 dev server 一開就是 HMR、一關就回內嵌 zip，**C++ 不用重 build**，改完存檔即時生效。
- `ui/vite.config.ts` 鎖 `server.host = 127.0.0.1`、`server.port = 5173`、`strictPort: true`，跟 probe 對齊，port 不會漂。
- native integration（slider / toggle / combo relay、`meterData`）是 document-start user script 注入，與載入來源無關，dev server 下參數與錶照常運作。
- `ui/src/routes/+layout.ts` 設 `ssr = false`：不設的話 dev server 會做 SSR，`@juce-framework/webview` 在 module 層讀 `window.__JUCE__`，直接 `window is not defined` → 整頁 500。
- 環境變數：`CLIP_DEV_UI_URL=<url>` 強制走指定 URL（任何 build type，例如 Release 也要 dev server 時）；`CLIP_DEV_UI_URL=off` 強制內嵌 zip、跳過 probe。
- 純瀏覽器開 `http://127.0.0.1:5173` 只會拿到 mock（`check_native_interop.ts`），沒有參數也沒有錶資料，只適合看版面。
- 開發期編輯器第一次打開可能出現 3 條 `juce_WebBrowserComponent.cpp:178` assertion，那是頁面還沒載完就送出第一筆 meter event，載完就不再出現，與 dev server 無關。
- 交付仍走 §8：`bun run build` → zip → `cmake --build build`。

## 9. Pre-Flight

- 零 em-dash / en-dash（日期範圍用連字號），零中點 `·`
- 單一 locked 暗主題、單一 accent（琥珀）、紅色僅語意
- 全域 `radius: 0`，唯一曲線＝圓形旋鈕／燈
- 全部文字對比 ≥ 4.5:1（主絲印 9.4:1、次級 5.15:1、琥珀 5.4:1）
- 無 CTA、無 eyebrow 群、無裝飾狀態點（僅 3 個真語意燈）、無版本字尾、無 locale 條、無 scroll cue、無圖例
- 動畫全部可一句說出動機；全部包 `prefers-reduced-motion`
- 無 `Inter`、無 AI 紫、無三等分卡片、無綠／青殘留色

## 10. 驗收紀錄

在 `http.server` 服 `Source/ui_dist`，用 Playwright 實測：

| 項目 | 結果 |
|---|---|
| DOM 座標 | panel `[0,0,1120,560]`、window `[32,64,1056,276]`、seam2 `y=354`、controls `[32,368,1056,162]`、knob `[32,368,236,112]`、readout `486`、main `502`、sub `517`（底 528） |
| 讀數不再壓旋鈕 | knob 底 480 → readout 頂 486，間 6px |
| 縮放 | `896×448 → scale(0.8)`、`1120×560 → scale(1)`、`1680×840 → scale(1.5)`；三者 `scrollWidth === innerWidth`（零捲動） |
| 渲染驗證 | 以 `backend.emitByBackend('meterData', ...)` 注入 512 點 `driven` 峰值 3.0 / `output` 已夾到 1.0：紅帶可見、meter 琥珀段與頂部紅段亮、`OVER` 燈與標籤轉紅 |
| 禁用資產 | `ui/src` 與 `Source/ui_dist` 無 `a9c891` / `8fd2e8`；無 `—` `–` `·`；CSS 無 `dark:` |
| lint / check / build | `bun run lint`（prettier＋eslint）、`bun run check`（0 error 0 warning）、`bun run build` 全過 |
| C++ | `cmake --build build -j` EXIT 0，0 error 0 warning，已安裝 VST3 |

**驗證註記**：本地要觸發 meter listener 必須用 `window.__JUCE__.backend.emitByBackend(id, JSON.stringify(payload))`；`emitEvent()` 是送往 native 的路徑，在瀏覽器裡是 no-op。Svelte 5 的 DOM 更新在 microtask，注入與讀取 DOM 必須分在不同 task。

---

## 11. 第三輪：Oversampling、ASYMMETRY、EMPHASIS、EQ view

### 11.0 決定（29 34，承 §1 的 28 題）

29. **Oversampling 做成可切** `1x / 2x / 4x / 8x`（四段開關），不是固定倍率。四段開關放上緣絲印條右側、BYPASS 左邊。
30. **不對稱削波**：clip 前加 bias、clip 後減去 `clipFn(bias)` 並歸一化，產生 even harmonics。DC 由輸出端 **AC-coupling（12 Hz 一階高通）** 拔掉，even harmonics 保留。`ASYM = 0` 時與第二輪行為完全相同。
31. **Pre-EQ emphasis**：固定 `3.0 kHz` 一階 high shelf，量由單一 `emphasis`（`0 … 10 dB`）控制；de-emphasis 是它在 z 域的**精確反轉**（分子分母互換）。兩者都在 oversampled wet path 內，淨響應恆等 1。
32. **EQ view 取代下方 waveform 窄帶**。`WAVE` / `EQ` 切換放微頂列**左側**（原本留空處）。168px 的 transfer curve 永遠在，版面零改動。
33. 控制列由五欄改**七欄**（加 `EMPHASIS`、`ASYMMETRY`）。
34. **（已完成）** Drive 的 skew 移除改線性；旋鈕 value arc 改用顯式 `A` command path。

> **34 的根因紀錄**：`vector-effect: non-scaling-stroke` 疊上 `stroke-dasharray` ＋ `pathLength` 時，Chrome 會在螢幕空間量 dash，掃角被除以 viewBox 縮放比。`isPointInStroke` 二分實測：Ø92（×0.92）應 270° 實得 **293.45°**、Ø112（×1.12）應 270° 實得 **241.11°**；拿掉該屬性兩者都回到 270°。故 `.knob-arc` / `.knob-groove` 不得再使用 dasharray。
>
> Drive 原本 `NormalisableRange(0, 24, 0.1, 3.0f)`：JUCE `convertFrom0to1` 給 `value = 24·n^(1/3)`，9 dB 只佔 normalised 5.27%，以 0.005/px 換算只有 10.5 px，手一甩就衝過。移除 skew 後 0 … 9 dB 佔 75 px，drag 與 arc 1:1。**副作用**：host automation 的 0–1 對應由 cubed 改線性。

### 11.1 參數（全部追加到 `createParameterLayout` 尾端，維持既有索引不動）

| ID | 型別 | 範圍 / 選項 | 預設 | `StringFromValue` | 狀態 |
|---|---|---|---|---|---|
| `emphasis` | `AudioParameterFloat` | `NormalisableRange(0, 10, 0.1)`，label `dB` | `0` | `` `+${value.toFixed(1)} dB` `` | **新增** |
| `asym` | `AudioParameterFloat` | `NormalisableRange(0, 1, 0.01)`，label `%` | `0` | `` `${Math.round(value * 100)} %` `` | **新增** |
| `oversampling` | `AudioParameterChoice` | `{"1x", "2x", "4x", "8x"}` | index `2`（4x） | 由 `ComboBoxState.properties.choices` 提供 | **新增** |

`drive` 改為 `juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f)`（**移除 skew**）。既有 `trim / mix / boost2x / shape / bypass` 不動。

### 11.2 引擎契約（`CMakeLists.txt`、`Source/**`）

**CMake**：`target_link_libraries` 加 `juce::juce_dsp`（`juce::dsp::Oversampling` 目前根本沒 link 到）。

**Relay / attachment（`PluginEditor.h` / `.cpp`）**：
```cpp
juce::WebSliderRelay emphasisRelay { ParameterIDs::emphasis };
juce::WebSliderRelay asymRelay      { ParameterIDs::asym };
juce::WebComboBoxRelay oversamplingRelay { ParameterIDs::oversampling };
// 三個都要 .withOptionsFrom(...) 並各自建
// WebSliderParameterAttachment / WebComboBoxParameterAttachment
```

**`ClipperSettings`** 追加：
```cpp
float emphasisDb = 0.0f;   // 0 … 10
float asym       = 0.0f;   // 0 … 1
int   oversampleIndex = 2; // 0=1x 1=2x 3=8x → 對應 choice index
```

**`ClipperDSP::prepare(double sampleRate, int maximumBlockSize, int numChannels)`**（簽名加 `numChannels`），在裡面建**四個** `juce::dsp::Oversampling<float>`：
```cpp
// JUCE 的 factor 參數是「指數」：2^factor 倍。0 → dummy（latency 0）。
idx0: Oversampling<float>(numChannels, 0, filterHalfBandFIREquiripple, true, true)  // 1x
idx1: Oversampling<float>(numChannels, 1, filterHalfBandFIREquiripple, true, true)  // 2x
idx2: Oversampling<float>(numChannels, 2, filterHalfBandFIREquiripple, true, true)  // 4x
idx3: Oversampling<float>(numChannels, 3, filterHalfBandFIREquiripple, true, true)  // 8x
// 每個都要 initProcessing(maximumBlockSize) 與 reset()
```
- FIR equiripple（stopband 起始 −90 / −75 dB，maxQuality）＋ `useIntegerLatency = true`，回報整數 latency。
- 公開 `getLatencySamples(int index)` 與 `getMaxLatencySamples()`。
- **更換倍率不做 crossfade**，接受短暫 click（罕見的使用者動作；host 本來就會因 latency 改變重新補償）。此為已知限制。

**處理鏈（`ClipperDSP::process`）**：
```
1. os = oversamplers[settings.oversampleIndex]
   up = os->processSamplesUp(AudioBlock<const float>(buffer))   // 回傳 oversampled block
2. 逐個 base sample n（smoother 每個 base sample 只推一次 → 時間常數維持 20ms 真實時間）：
     frame = advance(smoothedDriveGain, smoothedDoubleGain, smoothedMix,
                     smoothedShape, smoothedEmphasis, smoothedAsym)
     bias  = frame.asym * kAsymBias                 // kAsymBias = 0.20f
     t     = clipFn(bias, shape)                    // 見下方 clipFn
     invNorm = 1 / (1 + t)
     A     = Decibels::decibelsToGain(frame.emphasisDb)
     preB0 = A - (A - 1) * g[osIndex]      // g、gamma 在 prepare 依各倍率預先算好
     preB1 = A * gamma[osIndex] - (A - 1) * g[osIndex]
     逐個 oversampled sample k、逐 channel ch：
         up    = upBlock[ch][n * F + k]                  // dry 參考（emphasis 之前）
         w     = up * frame.driveGain * frame.doubleGain
         y     = preFilter[ch].process(w, preB0, preB1, gamma)   // emphasis
         b     = y + bias
         c     = (clipFn(b, shape) - t) * invNorm
         z     = postFilter[ch].process(c, preB0, preB1, gamma)  // de-emphasis
         upBlock[ch][n * F + k] = up + frame.mix * (z - up)      // 就地寫回
3. os->processSamplesDown(AudioBlock<float>(buffer))
4. 逐個 base sample、逐 channel：
     trimGain = smoothedTrimGain.getNextValue()
     buffer[ch][n] = hp[ch].process(buffer[ch][n]) * trimGain    // AC-coupling → trim
```

`clipFn(v, shape) = lerp(tanh(v), clamp(v, -1, 1), shape)`，與現況同一個函式。
`clipFn(bias, shape)` 中 `bias ≤ 0.20` 恆在 `[-1, 1]` 內，故 `clamp` 分支等於 `bias` 本身。

**Emphasis / de-emphasis 係數（精確反轉，一階 high shelf）**
```
kappa = 1 / tan(pi * kEmphasisCornerHz / fsOS)     // kEmphasisCornerHz = 3000.0f
gamma = (1 - kappa) / (1 + kappa)
g     = 1 / (1 + kappa)
E(z) = (preB0 + preB1 z^-1) / (1 + gamma z^-1)     // preB0 = A - (A-1)g, preB1 = A*gamma - (A-1)g
D(z) = (1 + gamma z^-1) / (preB0 + preB1 z^-1)     // 分子分母互換 = 精確 1/E
```
- `E(1) = 1`（DC 不變）、`E(-1) = A`（Nyquist 正好是 A）**代數上精確成立**，不是近似。
- `D` 的極點 = `E` 的零點，可證 `|b1/b0| < 1`，恆穩定。
- `emphasisDb = 0` → `A = 1` → `preB0 = 1, preB1 = gamma` → 兩段都退化成恆等，**完全 bypass**。
- `kappa / gamma / g` 只依 `fsOS` 與 `fc`，四個倍率各一組，`prepare()` 預先算好存成 array；每 base sample 只重算 `A / preB0 / preB1`。
- 濾波器狀態（`x1, y1`）**每 channel 各一份**，pre / post 各一份。
- 直接型 1：`y = b0*x + b1*x1 - a1*y1`（pre，`a1 = gamma`）；post 要除以 `preB0`：`y = (x + gamma*x1 - preB1*y1) / preB0`。

**AC-coupling（常駐，非參數）**
```
kAcCoupleHz = 12.0f，係數在 prepare 依 base sampleRate 算：
kappaHP = 1 / tan(pi * 12 / fs)
gammaHP = (1 - kappaHP) / (1 + kappaHP)
gHP     = 1 / (1 + kappaHP)
b0 = 1 - gHP, b1 = gammaHP - gHP, a1 = gammaHP
y  = b0*x + b1*x1 - a1*y1          // DC 處恰為 0，Nyquist 恆為 1
```
位置在 downsample 之後、trim 之前。**這是一階高通**，20 Hz 約 −1.0 dB、40 Hz 約 −0.36 dB。

**歸一化與輸出邊界（ASYM 的關鍵）**
- `t = clipFn(bias, shape)`，`invNorm = 1 / (1 + t)`
- `c = (clipFn(y + bias, shape) - t) * invNorm`
- 於是 `c ∈ [−1, (1 − t)/(1 + t)]`：**負向天花板恰為 −1，正向天花板低於 1**。
- `bias = 0` → `t = 0`、`invNorm = 1` → `c = clipFn(y)`，與現況完全相同。
- `asym = 1`、`shape = 1`：`t = 0.20`，正向天花板 `0.80 / 1.20 = 0.667`（−3.5 dB）。這是不對稱的必然代價，**不要**試圖補償。

**Latency 與 bypass**
- `ClipperSettings` 讀到 `oversampleIndex` 改變時，`setLatencySamples(getLatencySamples(index))`。
- `processBlock` 的 bypass 5ms crossfade，**參考訊號改成「延遲了 latency 的輸入」**，否則 bypass 輸出與已報告的 latency 不同步、crossfade 會打梳狀濾波。
  - 自建簡易循環緩衝（`std::vector<float>` × channel，長度 `maxLatency + maxBlockSize`）即可，**不要**用 `juce::dsp::DelayLine`（delay 0 有邊界問題）。
  - `latency == 0`（1x）時 read = 當前輸入。
  - **照舊**維持 `driven` / `output` waveform 記原始輸入（不記延遲版）：那是捲動歷史，幾 ms 位移看不出来。
- `OVER` 判定**不變**：仍以 `driven = input × (2x ? 2 : 1) × driveGain` 對 `±1.002 / ≤1.0 / 3 幀 / 200ms` 這套邏輯。emphasis 與 bias **不進** OVER 與紅帶（asym > 0 時正向其實會早一點被削，這是已知近似，asym = 0 時精確）。

### 11.3 Curve 公式（UI 端，**取代 §7.4**）

```
bias    = asym × 0.20
clipFn  = (v) => lerp(tanh(v), clamp(v, -1, 1), shape)
t       = clipFn(bias)
norm    = 1 + t
gain    = 10^(drive/20) × (boost2x ? 2 : 1)
c       = (clipFn(input × gain + bias) - t) / norm
out     = c × 10^(trim/20)
```
- **不含 `mix`**（維持 §7.4 假設 2）。
- **不含 emphasis**：emphasis 是動態濾波器，靜態轉移曲線畫它沒意義。
- **不含 AC-coupling**：曲線畫的是 waveshaper 特性；真把 DC blocker 算進去，DC 輸入會整條歸零，曲線沒意義。
- ceiling 線：正向 `y = trimGain`（標籤照舊 `${trimDb} dBFS`）；**`asym > 0` 時額外畫一條負向 `y = −trimGain`**（同一 `.ceiling-line` 樣式）。標籤不變，因為絕對天花板恆等 `trimGain`。

### 11.4 版面（**取代 §4.1 微頂列、§4.2 控制列**）

**上緣絲印條右側**（右對齊 `x = 1088`）新增 Oversampling 組，放在 BYPASS 組左邊：
```
[OVERSAMPLING 標籤 9px] [間8] [1x|2x|4x|8x  四段 4×34 = 136，高22] [間32] [lamp 6] [間6] [BYPASS] [間8] [搖臂 44×22]
```

**控制列改七欄，總寬 1056**：

| 欄 | 寬 | 內容 |
|---|---|---|
| DRIVE | 180 | 旋鈕 **Ø112** |
| 2x | 156 | 搖臂 `140×56` |
| MIX | 144 | 旋鈕 Ø92 |
| SHAPE | 144 | 旋鈕 Ø92 |
| EMPHASIS | 144 | 旋鈕 Ø92 |
| ASYMMETRY | 144 | 旋鈕 Ø92 |
| TRIM | 144 | 旋鈕 Ø92（降級） |

`180 + 156 + 144 × 5 = 1056`。順序按重要性：`DRIVE → 2x → MIX → SHAPE → EMPHASIS → ASYMMETRY → TRIM`（TRIM 是後級輸出，永遠最後）。垂直結構、三列文字基線、共用對齊**完全不動**（§4.2 的 368/486/502/517/528 全部照舊）。

**微頂列**改成 `justify-content: space-between`：
- 左：`WAVE` / `EQ` 視圖切換（兩個 9px mono 文字按鈕，**不是**實體控件，屬顯示器自身的模式鍵；active `--amber`、inactive `--dim`、hover `--ivory`，`radius: 0`、無背景、無邊框）
- 右：照舊 `OVER` 燈 ＋ `OVER` 標籤

**微底列**：
- 左（僅 EQ 視圖時出現）：`EMPH 3.0 kHz +7.5 dB`
- 右：照舊 ceiling 標籤

### 11.5 新增文案（零 em-dash / en-dash / 中點）

| 位置 | 文案 |
|---|---|
| EMPHASIS 主標 / 副標 / 讀數 | `EMPHASIS` / `PRE-EQ` / `+${v.toFixed(1)} dB` |
| ASYMMETRY 主標 / 副標 / 讀數 | `ASYMMETRY` / `EVEN / ODD` / `${Math.round(v * 100)} %` |
| 上緣 Oversampling 標籤 | `OVERSAMPLING` |
| 四段開關段名 | `1x` `2x` `4x` `8x` |
| 視圖切換 | `WAVE` `EQ` |
| 微底左（EQ 視圖） | `EMPH 3.0 kHz ${emphasis.toFixed(1)} dB`（帶 `+`） |

`ASYMMETRY` 主標在 12px / `0.18em` 下約 73px，144px 欄寬夠；`EVEN / ODD` 副標約 61px。若實測溢出，優先縮 letter-spacing，**不要**改文案。

### 11.6 四段開關與旋鈕的既有規格延伸

- 四段開關沿用 §4.4「凹槽＋凸帽」語彙：容器 = 凹槽（`#191B1D`、1px `--bezel-out`、`inset 0 1px 3px`），每段 = 凸帽；**被選中的那段用按下態**（`translateY(2px)` ＋高光降到 `.05` ＋ `inset 0 2px 5px`），與搖臂同一套視覺語言，**不加 lamp**。段名 9px IBM Plex Mono，選中 `--ivory`、未選 `--dim`。
- 無性別：`role="radiogroup"` ＋ 每段 `role="radio"` `aria-checked`；未註冊 relay 時整組 `disabled` 並降對比（比照 `.rocker:disabled`）。
- 新旋鈕完全沿用 §4.3 規格（270° 溝槽、3px arc、9 刻度、Ø92）。

### 11.7 EQ view（980 × 65，取代 wave 窄帶）

- **x 軸**：log 頻率 `20 Hz … 20 kHz`
  `x = (log10(f) - log10(20)) / 3 × 980`（跨度正好 3 個 decade）
- **y 軸**：`±12 dB`
  `y = 32.5 - dB × (65 / 24)`
- **曲線（用連續時間式，與取樣率無關，且與 11.2 的 z 域式在 20 kHz 內差異 < 1 dB）**
  ```
  A  = 10^(emphasis/20), fc = 3000
  preDb(f)  = 20 × log10( |1 + j·A·f/fc| / |1 + j·f/fc| )
  postDb(f) = -preDb(f)
  ```
  - **pre（emphasis，打進 clipper 的）**：`--amber` 2px，`.curve-line` 同款
  - **post（de-emphasis，回來的）**：`--dim` 1px、`stroke-dasharray: 4 4`、opacity .5
  - `emphasis = 0` 時兩條都重合在 0 dB 線上（視覺上等於只剩 graticule），**不要**為此加任何「空狀態」提示
- **graticule**：垂直線 `100 Hz / 1 kHz / 10 kHz` ＋ 水平 `0 dB` 線，沿用 `.graticule` 樣式（`--dim` 1px opacity .35）
- `emphasis = 10 dB` 時 20 kHz 約 `+9.9 dB` → `y ≈ 5.7`，不會溢出 0 … 65。

### 11.8 Svelte 端接法

```ts
import { getSliderState, getToggleState, getComboBoxState } from '@juce-framework/webview';

const emphasisState = getSliderState('emphasis');
const asymState      = getSliderState('asym');
// combo 要比照 resolveBypassState 的守衛：未註冊就回 null、四段開關 disabled
const oversamplingState = resolveOversamplingState();  // getComboBoxState('oversampling')
```
- `KnobName` 加 `'emphasis' | 'asym'`；`defaults` 加 `emphasis: 0, asym: 0`。
- `getVisualNormalised`：`emphasis → value / 10`、`asym → value`。
- `ariaRange`：`emphasis {min: 0, max: 10, now}`、`asym {min: 0, max: 100, now: value × 100}`。
- 四段開關用 `getChoiceIndex()` / `setChoiceIndex(i)`，`valueChangedEvent` 更新。
- **既有 `arcPath()` 是唯一允許的 arc 寫法**，禁止回到 circle + dasharray。

### 11.9 交付與驗收

命令序列不變（§8）：`cd ui && bun run lint && bun run check && bun run build` → zip → `cmake --build build -j`。zip 不會自動重建。

新增驗收項：
- DOM：controls 內七欄、寬度 `180/156/144/144/144/144/144`、readout/main/sub 仍在 `486/502/517`
- 上緣右側有四段開關，右端對齊 `x = 1088`
- 微頂左有 `WAVE` `EQ`、右仍是 `OVER`
- 切到 `EQ` 得到 980×65 的 emphasis 曲線；`emphasis` 拉到 10 時 20 kHz 端 y ≈ 5.7
- 四顆既有旋鈕＋兩顆新旋鈕 arc 終點 vs 指針角度誤差 `< 0.01°`
- `bun run lint` / `check` / `build` 全過；`cmake --build build -j` 0 error 0 warning
- 禁用資產複測：無 `a9c891` / `8fd2e8`、無 `—` `–` `·`、CSS 無 `dark:`

---

## 12. 第四輪：Emphasis 模式（OFF / TAPE / TUBE）

### 12.0 決定（35 38）

35. **`emphasis` 由 `0 … 10 dB` 改成 `0 … 100 %`**，同時映射增益與主頻率，`0 % = 0 dB` 完全平坦。**取代 §11.1 的 emphasis 定義**。副作用：既有 host automation / preset 的 0-1 對應改變（與 drive 移除 skew 同類，已接受）。
36. **新增 `emphasisMode` choice `OFF / TAPE / TUBE`，預設 `OFF`**。`OFF` = 全平坦；選 `TAPE` 或 `TUBE` 時該模式的 **HP、LP、low shelf 常開**（voicing，`0 %` 也作用），**只有 shelf/bell 的增益與主頻率受 EMPH 控制**。
37. **MODE 三段開關放上緣絲印條右群最左側**，右對齊 `x = 1088`，順序 `MODE → OVERSAMPLING → BYPASS`，沿用四段開關的凹槽＋凸帽語彙。
38. **EQ view 維持兩條**：琥珀實線 = pre-EQ（撞進 clipper 的）、灰虛線 = post-EQ（拉回來的）。**取代 §11.5 微底左文案與 §11.7 的曲線公式**。

**控制列零改動**：七欄、寬度、`486/502/517/528` 基線全部照 §11.4 不動。EMPHASIS 維持單一旋鈕。

### 12.1 參數

| ID | 型別 | 範圍 / 選項 | 預設 | `StringFromValue` |
|---|---|---|---|---|
| `emphasis` | `AudioParameterFloat` | `NormalisableRange(0, 100, 1)`，label `%` | `0` | `` `${Math.round(v)} %` `` |
| `emphasisMode` | `AudioParameterChoice` | `{"OFF", "TAPE", "TUBE"}` | index `0` | 由 `choices` 提供 |

兩個都追加到 `createParameterLayout` **最尾端**（`oversampling` 之後）。`emphasisMode` 走 `WebComboBoxRelay` + `WebComboBoxParameterAttachment`，比照 `oversampling`。

### 12.2 模式定義

令 `p = smoothedEmphasis / 100`（`p ∈ [0,1]`，平滑 20 ms，`smoothedEmphasis ≥ 0`）。
`g = juce::Decibels::decibelsToGain(p × maxDb)`（**線性 gain**，JUCE 的 `ArrayCoefficients` 收線性值，內部 `A = sqrt(gain)`）。
所有模式濾波器跑在 **oversampled domain**（`fsOS` = 當前倍率的取樣率），在 clip 之前／之後、**mix 之前**。
Q 一律用 `0.70710678118654752440`（= `Coefficients::inverseRootTwo`，但那是 `private`，實作用同值常數即可），bell 的 Q 另行掃描。設計公式用 **`juce::dsp::IIR::ArrayCoefficients<float>`**（命名空間有 `IIR` 一層；回 `std::array<float,6>`、零配置）。

| 模式 | Pre-EQ（由前而後） | Post-EQ（由前而後） |
|---|---|---|
| `OFF` | 無（整段跳過） | 無（整段跳過） |
| `TAPE` | 1. `makeHighPass(fsOS, 25.0f)`<br>2. `makeHighShelf(fsOS, 2500 + 1000·p, Q, g)`，`maxDb = 8.0f` | 1. shelf 的**精確反轉**（§12.3）<br>2. `makeLowShelf(fsOS, 75.0f, Q, decibelsToGain(2.25f))`（head bump，常開） |
| `TUBE` | 1. `makeHighPass(fsOS, 115.0f)`<br>2. `makePeakFilter(fsOS, 1500 + 1500·p, 0.7f + 0.3f·p, g)`，`maxDb = 6.0f` | 1. bell 的**精確反轉**（§12.3）<br>2. `makeLowPass(fsOS, 14000.0f)`<br>3. `makeLowShelf(fsOS, 100.0f, Q, decibelsToGain(3.0f))` |

常數（全部取自 §11 使用者給定區間的**中點**，不可自行調整）：

```
kTapeHpHz        = 25.0f    // 20 … 30 的中點，12 dB/oct
kTapeBumpHz      = 75.0f    // 60 … 90 的中點
kTapeBumpDb      = 2.25f    // 1.5 … 3 的中點
kTapeShelfMinHz  = 2500.0f
kTapeShelfSpanHz = 1000.0f  // 2500 → 3500
kTapeMaxDb       = 8.0f     // 0 → +8 dB，50% 正好落在 +4 dB

kTubeHpHz        = 115.0f   // 80 … 150 的中點，12 dB/oct
kTubeLpHz        = 14000.0f // 12 … 16 的中點，12 dB/oct
kTubeLowShelfHz  = 100.0f
kTubeLowShelfDb  = 3.0f     // 2 … 4 的中點
kTubeBellMinHz   = 1500.0f
kTubeBellSpanHz  = 1500.0f  // 1500 → 3000
kTubeBellQMin    = 0.7f
kTubeBellQSpan   = 0.3f     // 0.7 → 1.0
kTubeMaxDb       = 6.0f     // 0 → +6 dB，50% 正好落在 +3 dB
```

**skipping 規則**：`gainDb = p × maxDb`；當 `gainDb < 0.01f` 時，**shelf/bell 那一對 pre 與 post 都整個跳過**（不建係數、不跑 biquad、狀態保持 0）。`HP / LP / low shelf` 不受此規則影響，照常跑。這樣 `EMPH = 0 %` 的 shelf/bell 階段是**真・零運算**而非「identity biquad 的浮點誤差」。

**HP / LP / low shelf 的係數**：頻率固定、只依 `fsOS`，在 `prepare()` 依四個倍率各預先算好四組存起來（與 §11.2 的 `kappa/gamma/g` 同樣手法）。
**shelf/bell 的係數**：`p` 變動即變動，每個 base sample 重算一次（`ArrayCoefficients` 回 `std::array<float,6>`，零配置；`makeHighShelf` / `makePeakFilter` 各兩三個 `sin`/`cos`，48k 取樣下可忽略）。

### 12.3 精確反轉（swap 規則）

`ArrayCoefficients` 回傳順序 `{b0, b1, b2, a0, a1, a2}`。

pre（正規化到 `a0 = 1`）的傳遞函數 `N(z) = (b0 + b1z⁻¹ + b2z⁻²) / (a0 + a1z⁻¹ + a2z⁻²)`。
它的精確反轉就是分子分母互換：

```
post = { a0/b0, a1/b0, a2/b0,  1,  b1/b0, b2/b0 }
```

差分方程（兩者同一個結構，`a0` 已正規化）：
```
pre :  y = b0·x + b1·x1 + b2·x2 − a1·y1 − a2·y2      // 係數已除過 a0
post:  y = b0'·x + b1'·x1 + b2'·x2 − a1'·y1 − a2'·y2  // a0' = 1，不用除
```

- `N(z) × N⁻¹(z) = 1` 是**代數恆等**，與 JUCE 用哪套設計公式無關。已另行推導確認 JUCE 的 `makeHighShelf` 與 `makePeakFilter` 在 `gainFactor → 1/gainFactor` 時係數恰好互換，所以直接拿 `1/gain` 再設計一次也是等價的；**但一律用上面的 swap**，只需設計一次、也保證精確。
- **穩定性**：`post` 的極點 = `pre` 分子的根。實測（tape shelf 3 kHz @ 48 kHz、`maxDb = 8`、`Q = 0.7071`）`q = b2/b0 = 0.569`、`1+p+q = 0.077 > 0`、`1−p+q = 3.06 > 0`、`|q| < 1`，兩根都在單位圓內（`|z| = √q = 0.754`）。`EMPH` 掃到 0 / 100 % 兩端都要複驗。
- **HP / LP / low shelf 沒有反轉**：它們是 voicing，不是抵銷對。

### 12.4 模式切換的 5 ms crossfade

換 `emphasisMode` 會讓 HP / bump 突然進出，**一定會 click**（tape bump +2.25 dB 是聽得出來的台階）。做法：

- 準備 **兩組 bank**（`bankA` / `bankB`），各自持有完整的 pre 狀態與 post 狀態（每 channel 一份）。
- 穩定時只跑當前那一組。
- 參數讀到 `emphasisMode` 改變：把「新的那組」bank 的所有狀態**歸零**，開始 5 ms 線性 crossfade。
- **crossfade 分兩處做，clipper 只跑一次**（避免整段非線性跑兩次）：
  ```
  preOut = lerp(bankOld.pre(w), bankNew.pre(w), fade)
  c      = clip(preOut)                       // 唯一一次
  out    = lerp(bankOld.post(c), bankNew.post(c), fade)
  ```
  `fade ∈ [0,1]`，每個 **base sample** 推一次（oversampled 內重用同一個值，8 倍下階梯 20.8 µs，聽不出來），`0.005 × sampleRate` 個 base sample 走完。
- `fade = 0` 時輸出恰為舊鏈、`fade = 1` 時恰為新鏈；中間兩條都是有界訊號，`mix < 100 %` 不會因此出梳狀濾波。
- **fade 進行中又切 mode**：把正在 fade-in 的那個 bank **晉升成「舊 bank」**，然後重新起一個完整 5 ms fade。不要跳回上一個 chain（那會造成一次不連續）。
- fade 結束後交換 active 指標，另一組閒置待命（下次切換時歸零）。
- `OFF → TAPE` 的「舊鏈」就是什麼都不做，`bankOld` 的 pre/post 都設定成跳過即可。

**這是本輪唯一的非線性鏈重構**，`ClipperDSP::process` 的 `pre → clip → post` 三段式改成「兩組 pre 混合 → 單次 clip → 兩組 post 混合」，穩定（非 fade）狀態下等價於只跑一組。

### 12.5 處理鏈（其餘與 §11.2 完全相同）

```
1. processSamplesUp
2. 逐 base sample 推 smoother（drive / double / mix / shape / emphasis / asym），
   並在 mode≠OFF 且 gainDb ≥ 0.01 時重算 shelf/bell 係數
3. 逐 oversampled sample、逐 channel：
     w     = up × driveGain × doubleGain
     preOut = lerp(bankOld.pre(w), bankNew.pre(w), fade)     // OFF 時 pre 為恆等
     b      = preOut + bias
     c      = (clipFn(b, shape) − t) × invNorm               // §11.2 不變
     z      = lerp(bankOld.post(c), bankNew.post(c), fade)
     upBlock = up + mix × (z − up)
4. processSamplesDown
5. 逐 base sample：12 Hz AC-coupling 高通 → trim        // §11.2 不變
```

- **ASYM 的 bias / 歸一化、AC-coupling、oversampling、latency、bypass 延遲參考、OVER 判定、waveform 錄製：全部不動。**
- **post 的 low shelf 可能讓輸出略超 0 dBFS**（clip 後的 wet 峰值 ±1，再被抬 +2.25 / +3 dB）。這是磁帶 head bump 的固有行為，**不加第二道 clamp**；需要時用 `TRIM` 壓回。此為已知限制。

### 12.6 UI

**上緣右群**（右對齊 `x = 1088`）新增 MODE 組，插在 OVERSAMPLING 左邊：
```
[MODE 標籤 9px] [間8] [OFF|TAPE|TUBE  3×34 = 102，高22] [間32]
[OVERSAMPLING 標籤] [間8] [1x|2x|4x|8x 136，高22] [間32]
[lamp 6] [間6] [BYPASS] [間8] [搖臂 44×22]
```
- 完全沿用 `.os-switch` / `.os-seg` 的凹槽＋凸帽樣式，只改段數；段名 `OFF` `TAPE` `TUBE`，9px IBM Plex Mono，選中 `--ivory`、未選 `--dim`，被選中那段用按下態，**不加 lamp**。
- `role="radiogroup"` ＋ 3 × `role="radio" aria-checked`；relay 未註冊時整組 `disabled` 降對比（比照 §11.6）。
- 粗估寬度：MODE 組約 140、OVERSAMPLING 組約 239、BYPASS 組約 114，總計約 557 → 左緣約 `x = 531`，品牌 lockup 結束約 `x = 175`，中間仍有約 350px 空隙，**不會撞**。

**旋鈕**：
- `formatKnob('emphasis')` → `` `${Math.round(v)} %` ``（與 `StringFromValue` 一致，**不再出現 `+` 與 `dB`**）
- `getVisualNormalised('emphasis')` → `value / 100`（原 `/ 10`）
- `ariaRange('emphasis')` → `{ min: 0, max: 100, now: v }`
- `defaults.emphasis` 仍是 `0`

**EQ view（980 × 65）**：
- **移植 JUCE 的五組設計公式到 TS**，共用 `abs(H(e^jω))` 計算，`fsRef = 48000`（UI 拿不到實際取樣率；bilinear 已對角頻率預扭，20 kHz 內與實際倍率差異可忽略）：
  `makeHighPass(fs, f)`、`makeLowPass(fs, f)`、`makeHighShelf(fs, f, Q, gain)`、`makeLowShelf(fs, f, Q, gain)`、`makePeakFilter(fs, f, Q, gain)`。
  公式照抄 `JUCE/modules/juce_dsp/processors/juce_IIRFilter.cpp` 第 84、110、208、234、260 行，回傳同序 `{b0,b1,b2,a0,a1,a2}`，**post 用 §12.3 的 swap**。
  `gain` 是**線性**值（`10^(dB/20)`），Q 用 `0.70710678118654752440`。
- `preDb(f)` = 該模式 pre 各階段 `dB` 相加；`postDb(f)` = post 各階段相加。順序對量值無影響。
- x 軸維持 `x = (log10(f) − log10(20)) / 3 × 980`，`f ∈ [20, 20000]`；y 維持 `y = 32.5 − dB × (65 / 24)`（±12 dB）。
- **y 必須 clamp 到 `[0, 65]`**：tube 的 `HP 115 Hz` 在 20 Hz 處約 −25 dB，不 clamp 會把 path 甩出 viewBox。
- `OFF` 時兩條都落在 `y = 32.5`（`pre/post` 都跳過）；`EMPH = 0 %` 時 shelf/bell 貢獻 0 dB，但仍看得到 `HP / LP / low shelf` 的 voicing（這正是 §12.0 決定 36 想讓人看到的差異）。
- 樣式：`.curve-line`（琥珀 2px，含 `vector-effect: non-scaling-stroke`）＝pre；`.eq-post`（灰 1px、`stroke-dasharray: 4 4`、opacity .5、**不含** `vector-effect`）＝post。graticule 照舊（`100 Hz / 1 kHz / 10 kHz` 垂直 ＋ `0 dB` 水平）。

**微底列左（僅 EQ 視圖）**，取代 §11.5 的 `EMPH 3.0 kHz …`：
```
OFF  → "EMPH OFF"
TAPE → "TAPE 3.0 kHz +4.0 dB"      // 頻率取 2500+1000·p，增益取 p×8.0
TUBE → "TUBE 2.3 kHz +3.0 dB"      // 頻率 1500+1500·p、增益 p×6.0；p=0.5 → 2.25 kHz 進位成 2.3
```
頻率一律 `f >= 1000 ? (f/1000).toFixed(1) + ' kHz' : f.toFixed(0) + ' Hz'`（本輪兩個模式的頻率恆 ≥ 1500，實際只會走 kHz 分支）。增益帶 `+`，`toFixed(1)`。微底右的 ceiling 標籤照舊。

**curve 公式（§11.3）不變**：emphasis 是動態濾波器，靜態轉移曲線照樣不含它，也不含 mode voicing。

### 12.7 已知限制

1. `emphasis` 範圍由 `0…10 dB` 改 `0…100 %`，舊 preset / automation 對應改變。
2. post 的 low shelf 讓輸出可能略超 0 dBFS，不加二次 clamp，用 `TRIM` 補。
3. EQ view 用 `fsRef = 48000` 繪製；若 host 跑 44.1 / 96 kHz，20 kHz 附近（主要影響 tube 的 14 kHz low pass）會有些許偏差。
4. 更換 oversampling 倍率仍不 crossfade（§11.2 已知限制）；**模式切換則有 5 ms crossfade**，兩者策略不同是刻意的（模式是會被 A/B 的動作）。

### 12.8 驗收（疊加在 §11.9 之上）

- `emphasisMode = OFF` 時 mode 濾波器**零運算**（不建係數、不跑 biquad），wet 路徑完全退化成 §11.2 的鏈：`oversample → clip(asym) → mix → downsample → 12 Hz AC → trim`；與 `EMPH = 0 %` 無關（`OFF` 一律整段跳過）。
- `EMPH = 50 %` 時的**設計參數**：tape shelf `+4.0 dB` / `3.0 kHz`；tube bell `+3.0 dB` / `2.25 kHz` / `Q = 0.85`。
  注意 shelf 的語意：JUCE `makeHighShelf` 在**轉角 3.0 kHz 實測約 +2.0 dB（設計值的一半）**，到 20 kHz 才達 `+4.0 dB`（實測已驗證）。微底文案顯示**設計參數**即可，這是 shelf 的標準讀法。tube bell 則在 2.25 kHz 實測恰 `+3.0 dB`。
- 交換 swap 係數後 `N(z) × N⁻¹(z)` 在掃頻格點上誤差 `< 1e-5`（用 `EMPH = 1 % / 50 % / 100 %`、tape / tube 各測一次）。
- DOM：上緣右群含 MODE 三段，右端 `x = 1088`，左緣不早於 `x = 480`；控制列仍是七欄、基線不動。
- 切 `EQ` 後 `OFF` 是平的 0 dB；切 `TAPE` 看得到 25 Hz HP 與 75 Hz bump；切 `TUBE` 看得到 115 Hz HP、bell、14 kHz LP、100 Hz bump；衰減區（HP / LP 掉到 −12 dB 以下）會被 clamp 在畫布底邊 `y = 65`，**全 path 的 y 必須落在 `[0, 65]`**。
- `EMPHASIS` 讀數是 `%`；arc 終點 vs 指針誤差 `< 0.01°`。
- `bun run lint` / `check` / `build` 全過；`cmake --build build -j` 0 error 0 warning；禁用資產複測同 §11.9。
