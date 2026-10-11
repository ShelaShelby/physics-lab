# Installs the Physics Lab auto-installer for the current user (no admin): copies the scripts to
# %LOCALAPPDATA%\PhysicsLabAutoInstall, puts a shortcut in the Startup folder and starts the watcher now.
$ErrorActionPreference = 'Stop'
$target = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'PhysicsLabAutoInstall'
New-Item -ItemType Directory -Path $target -Force | Out-Null
foreach ($f in 'PhysicsLabAutoInstall.ps1', 'Uninstall.ps1', 'CleanOld.ps1', 'README.txt') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $f) -Destination $target -Force
}
$watcher = Join-Path $target 'PhysicsLabAutoInstall.ps1'
$psArgs = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}"' -f $watcher

$lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'PhysicsLab AutoInstall.lnk'
$sc = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sc.TargetPath = 'powershell.exe'
$sc.Arguments = $psArgs
$sc.WorkingDirectory = $target
$sc.WindowStyle = 7
$sc.Description = 'Installs new Physics Lab downloads'
$sc.Save()

Start-Process -FilePath 'powershell.exe' -ArgumentList $psArgs -WindowStyle Hidden
Write-Host "Done. The watcher is running and will start with Windows. Shortcut: $lnk"
