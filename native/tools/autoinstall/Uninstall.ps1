# Removes the Startup shortcut and stops the watcher. C:\PhysicsLab and the app are not touched.
$ErrorActionPreference = 'Stop'
$lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'PhysicsLab AutoInstall.lnk'
if (Test-Path -LiteralPath $lnk) { Remove-Item -LiteralPath $lnk -Force }
Get-CimInstance Win32_Process -Filter "Name = 'powershell.exe'" |
    Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -like '*PhysicsLabAutoInstall.ps1*' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
Write-Host 'Removed. You can delete the folder %LOCALAPPDATA%\PhysicsLabAutoInstall if you like.'
