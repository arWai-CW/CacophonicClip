-- Installs Cacophonic Clip into the macOS audio plug-in folders of the current user.
--
-- Why this exists: an unsigned .vst3 or .component that macOS quarantines does
-- not get an "Open" item in the Finder context menu, so the user would have to
-- run xattr in the Terminal. A quarantined .command script is blocked the same
-- way (open does nothing at all). Wrapping the install in an .app is the only
-- container that still gets the "Open" override, so the user can install
-- without ever touching the Terminal.
--
-- Installs for the current user only (~/Library), never /Library: an unsigned
-- installer that writes to a machine-wide folder with root rights is a much
-- bigger blast radius, and it would need an admin password. An
-- "install for all users" option existed and was dropped because the
-- administrator privileges escalation did not complete on macOS 27.
--
-- Why the quarantine attribute is cleared after copying: ditto preserves it by
-- default (neither --noqtn nor COPYFILE_DISABLE=1 stops it), so without
-- xattr -cr the install would still be blocked.

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
		display dialog "Installation failed:" & return & return & errMsg buttons {"OK"} default button 1 with icon caution
		quit
		return
	end try

	set doneMsg to "Installed. Both VST3 and AU are in place." & return & return & ¬
		"Quit and reopen your DAW (Cmd+Q) so it rescans the plug-in list." & return & return & ¬
		"Installed in:" & return & ¬
		"  " & vst3Dir & return & ¬
		"  " & auDir
	display dialog doneMsg buttons {"OK"} default button 1
	quit
end run

-- rm before ditto so leftovers from a previous version cannot survive.
-- xattr -cr drops the quarantine attribute; codesign --verify turns a broken
-- install into a failed install rather than a half-installed one.
on installScript(vst3Src, auSrc, vst3Dir, auDir, vst3Name, auName)
	return "set -e" & ¬
		" && mkdir -p " & (quoted form of vst3Dir) & " " & (quoted form of auDir) & ¬
		" && rm -rf " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && ditto " & (quoted form of vst3Src) & " " & (quoted form of (vst3Dir & "/" & vst3Name)) & ¬
		" && ditto " & (quoted form of auSrc) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && xattr -cr " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName)) & ¬
		" && codesign --verify --strict " & (quoted form of (vst3Dir & "/" & vst3Name)) & " " & (quoted form of (auDir & "/" & auName))
end installScript
