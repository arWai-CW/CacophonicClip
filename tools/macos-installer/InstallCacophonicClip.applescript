-- 安裝 Cacophonic Clip 到目前登入使用者的音訊外掛資料夾。
--
-- 存在的理由：未簽章的 .vst3 / .component 被 Gatekeeper 擋下時，Finder 的右鍵選單
-- 不會出現「打開」，使用者只能自己開終端機跑 xattr。實測連 .command 腳本也一樣被擋。
-- 改成 .app 容器之後，使用者按右鍵 → 打開（或到系統設定 → 隱私權與安全性按「仍要打開」）
-- 就能裝完，全程不需要碰終端機。
--
-- 只裝給目前登入的使用者（~/Library），不碰 /Library：
--   未簽章的安裝包被 Gatekeeper 放行後就已經等於讓任意網站端出程式碼在跑，
--   寫進全機共用的 /Library 風險太大，而且需要管理員密碼。
--   曾經做過「安裝給所有使用者」的選項，實測在 macOS 27 上授權流程走不完，已拿掉。
--
-- 為什麼要清 quarantine：ditto 預設會保留來源的 quarantine 屬性
-- （實測 --noqtn 與 COPYFILE_DISABLE=1 都擋不掉），不清掉就等於沒裝。

on run
	set pluginName to "Cacophonic Clip"
	set vst3Name to pluginName & ".vst3"
	set auName to pluginName & ".component"
	set basePath to (POSIX path of (path to home folder))
	set vst3Dir to basePath & "Library/Audio/Plug-Ins/VST3"
	set auDir to basePath & "Library/Audio/Plug-Ins/Components"

	try
		set vst3Src to (POSIX path of (path to resource vst3Name))
		set auSrc to (POSIX path of (path to resource auName))
		do shell script installScript(vst3Src, auSrc, vst3Dir, auDir, vst3Name, auName)
	on error errMsg
		display dialog "安裝失敗：" & return & return & errMsg buttons {"好"} default button 1 with icon caution
		quit
		return
	end try

	set doneMsg to "安裝完成，VST3 與 AU 都裝好了。" & return & return & ¬
		"請完全關閉再開你的 DAW（Cmd+Q），讓它重新掃描外掛。" & return & return & ¬
		"安裝位置：" & return & ¬
		"  " & vst3Dir & return & ¬
		"  " & auDir
	display dialog doneMsg buttons {"好"} default button 1
	quit
end run

-- 先 rm 再 ditto，避免上一版殘留的檔案混進來。
-- xattr -cr 拿掉 quarantine，codesign --verify 失敗就整個安裝視為失敗，不要留半套。
on installScript(vst3Src, auSrc, vst3Dir, auDir, vst3Name, auName)
	return "set -e" & ¬
		" && mkdir -p " & (quoted form of vst3Dir) & " " & (quoted form of auDir) & ¬
		" && rm -rf " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && ditto " & (quoted form of vst3Src) & " " & (quoted form of (vst3Dir & "/" & vst3Name)) & ¬
		" && ditto " & (quoted form of auSrc) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && xattr -cr " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && codesign --verify --strict " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName))
end installScript
