# Download auto-installer for Windows (tools only, no app code changed)

## What changed
- New native/tools/autoinstall/: PhysicsLabAutoInstall.ps1 (watcher), Install.ps1, Uninstall.ps1, CleanOld.ps1, README.txt.
- Watcher: polls Downloads (known-folder API) every 3 s for PhysicsLab-windows-x64*.zip; ignores zips present at start; waits for no .crdownload/.part/.tmp, size unchanged over two polls, exclusive read; closes PhysicsLab.exe running from C:\PhysicsLab (CloseMainWindow, 5 s, Stop-Process); extracts (flat or one top folder; needs PhysicsLab.exe inside); swaps via C:\PhysicsLab.new / .old with rollback; starts the exe; deletes the zip; logs to C:\PhysicsLab\autoinstall.log (carried into the new build). One bad zip is logged and left alone. Single instance by named mutex.
- Settings: the app saves them in %APPDATA%\PhysicsLab\PhysicsLab\settings.txt (SDL_GetPrefPath, ui/settings.cpp), so nothing next to the exe needs keeping; README says so.
- Install.ps1 copies the scripts to %LOCALAPPDATA%\PhysicsLabAutoInstall (so they survive a Downloads cleanup), makes the Startup shortcut and starts the watcher.
- CleanOld.ps1 lists only (Downloads\PhysicsLab-cleanup-list.txt); -Confirm sends the listed items to the Recycle Bin.
- CI: the release step also zips the folder as PhysicsLab-AutoInstall.zip and attaches it to "latest" (app zip unchanged).
- Docs: RESUME.md line, OWNER-TODO.md line. changelog.cpp untouched (tool, not app).

## Tested (Linux, pwsh 7.4.6, temp folders, fake zips) - native/tests/autoinstall_test.ps1, 17 checks pass
- flat zip and one-top-folder zip install; old build replaced; log carried over; no .old/.new left.
- zip without exe: error, old build intact, zip left. Simulated failure of the final move: old build restored.
- zip pattern, .crdownload partner, watcher loop (old zip ignored, new zip installed and deleted, log line), second instance exits, CleanOld lists and deletes nothing.
- All five scripts parse with no errors. PSScriptAnalyzer could not be installed (module not reachable).

## Untested on Windows (Alex to try)
- Startup shortcut and autostart; hidden window; Install/Uninstall; stopping the watcher by command line.
- Downloads known-folder lookup; real browser .crdownload timing; Windows Expand-Archive.
- Closing the real PhysicsLab.exe and starting the new one; moving C:\PhysicsLab while files are locked.
- Recycle Bin in CleanOld -Confirm (not run, even on Linux). Windows PowerShell 5.1 specifics.
- The release attachment: seen only after the CI run.
