#!/bin/bash
# 把 InstallCacophonicClip.applescript 編譯成可分發的 .app，並把外掛本體塞進 Resources。
#
# 用法：
#   tools/macos-installer/build-installer.sh <外掛產物目錄> <輸出 .app 路徑>
#
# 產物目錄要同時有：
#   VST3/Cacophonic Clip.vst3
#   AU/Cacophonic Clip.component
#
# 為什麼外掛要放在 .app 的 Contents/Resources 裡：
#   讓安裝包自包含。使用者只需要對 dmg 裡這一個 .app 按右鍵 → 打開，
#   不依賴 dmg 掛載後的路徑，也不怕使用者先關掉 dmg。
#
# 為什麼不用 .pkg：
#   實測未簽章的 pkg 在 macOS 27 上直接被 Installer 判定「與您的 Mac 版本不相容」而中止，
#   根本裝不起來。就算能裝，未簽章的 pkg 被 Gatekeeper 放行後是以 root 權限寫進 /Library，
#   任何網站都能端出這種安裝包，使用者只不過是點了「仍要打開」。
#   .app 只寫 ~/Library，風險小很多，也不用管理員密碼。

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <artefacts-dir> <output.app>" >&2
    exit 1
fi

ARTEFACTS="$1"
OUTPUT="$2"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VST3="$ARTEFACTS/VST3/Cacophonic Clip.vst3"
AU="$ARTEFACTS/AU/Cacophonic Clip.component"

for f in "$HERE/InstallCacophonicClip.applescript" "$VST3" "$AU"; do
    if [ ! -e "$f" ]; then
        echo "missing: $f" >&2
        exit 1
    fi
done

rm -rf "$OUTPUT"
osacompile -o "$OUTPUT" "$HERE/InstallCacophonicClip.applescript"

# 背景執行，不要在 Dock 跳動
/usr/libexec/PlistBuddy -c "Add :LSUIElement bool true" "$OUTPUT/Contents/Info.plist" 2>/dev/null || true

ditto "$VST3" "$OUTPUT/Contents/Resources/Cacophonic Clip.vst3"
ditto "$AU" "$OUTPUT/Contents/Resources/Cacophonic Clip.component"

# applet 自己也要有簽章，否則 Gatekeeper 連「右鍵 → 打開」都不給。
# 這裡是 ad-hoc，之後拿到 Developer ID 要在 package-mac job 換成正式簽章加公證。
codesign --force --sign - "$OUTPUT" 2>/dev/null || codesign --force --sign - "$OUTPUT"

echo "built: $OUTPUT"
du -sh "$OUTPUT"
