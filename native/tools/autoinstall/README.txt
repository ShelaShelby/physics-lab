Physics Lab auto-installer (Windows, no admin needed)
1. Unzip PhysicsLab-AutoInstall.zip anywhere. Right-click Install.ps1 -> "Run with PowerShell".
2. From then on, every new PhysicsLab-windows-x64 zip you download is installed by itself into C:\PhysicsLab, started, and the zip is deleted.
   Zips already in Downloads when it starts are ignored. A broken zip is left alone; see C:\PhysicsLab\autoinstall.log.
3. Your settings are kept: the app saves them in %APPDATA%\PhysicsLab\PhysicsLab, not in C:\PhysicsLab.
4. To remove it: right-click Uninstall.ps1 -> "Run with PowerShell". The app in C:\PhysicsLab stays.
5. To tidy old downloads: run CleanOld.ps1 (it only makes a list in Downloads). To really clean up, run it
   in PowerShell as:  .\CleanOld.ps1 -Confirm   (items go to the Recycle Bin, never deleted for good).
If Windows blocks the scripts, right-click the zip -> Properties -> Unblock before unzipping.
