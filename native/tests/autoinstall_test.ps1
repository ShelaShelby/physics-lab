# Linux/pwsh test of the auto-installer logic (extract, replace, rollback, patterns, watcher loop, CleanOld list) in a temp folder.
# It does NOT test Windows things (Startup shortcut, closing the real exe, Recycle Bin, Downloads known folder).
# Run: pwsh -NoProfile -File native/tests/autoinstall_test.ps1
$ErrorActionPreference = 'Stop'
$tools = Join-Path $PSScriptRoot '../tools/autoinstall'
$root = Join-Path ([IO.Path]::GetTempPath()) ('pl-ai-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$fails = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:fails++ } }

$inst = Join-Path $root 'PhysicsLab'; $dl = Join-Path $root 'Downloads'
New-Item -ItemType Directory -Path $dl | Out-Null
. (Join-Path $tools 'PhysicsLabAutoInstall.ps1') -NoRun -NoStart -InstallDir $inst -WatchDir $dl

function Make-Zip([string]$name, [string]$kind, [string]$tag) {
    $src = Join-Path $root ('src-' + [guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $src | Out-Null
    $c = $src
    if ($kind -eq 'folder') { $c = Join-Path $src 'PhysicsLab'; New-Item -ItemType Directory -Path $c | Out-Null }
    if ($kind -ne 'noexe') { Set-Content (Join-Path $c 'PhysicsLab.exe') $tag } else { Set-Content (Join-Path $c 'other.txt') $tag }
    New-Item -ItemType Directory -Path (Join-Path $c 'font-licences') | Out-Null
    Set-Content (Join-Path $c 'font-licences/OFL.txt') 'x'
    $z = Join-Path $dl $name
    Compress-Archive -Path (Join-Path $src '*') -DestinationPath $z -Force
    return $z
}

# first install, zip without top folder
$z1 = Make-Zip 'PhysicsLab-windows-x64.zip' 'flat' 'v1'
Install-Build $z1
Check ((Get-Content (Join-Path $inst 'PhysicsLab.exe')) -eq 'v1') 'flat zip installed'
Write-InstallLog 'a.zip' 'OK'
# replace, zip with one top folder; old log carried over, no .old/.new left
$z2 = Make-Zip 'PhysicsLab-windows-x64 (1).zip' 'folder' 'v2'
Install-Build $z2
Check ((Get-Content (Join-Path $inst 'PhysicsLab.exe')) -eq 'v2') 'one-top-folder zip replaces'
Check (Test-Path (Join-Path $inst 'autoinstall.log')) 'log carried over'
Check (-not (Test-Path "$inst.old") -and -not (Test-Path "$inst.new")) 'no .old/.new left'
# bad zip (no exe): error, old build intact
$z3 = Make-Zip 'PhysicsLab-windows-x64 (2).zip' 'noexe' 'bad'
$threw = $false; try { Install-Build $z3 } catch { $threw = $true }
Check ($threw -and (Get-Content (Join-Path $inst 'PhysicsLab.exe')) -eq 'v2') 'zip without exe fails, v2 untouched'
Check (Test-Path $z3) 'bad zip left in place'
# failure during the swap: restore the old folder
$z4 = Make-Zip 'PhysicsLab-windows-x64 (3).zip' 'flat' 'v4'
function Move-Item { param($LiteralPath, $Destination, $ErrorAction) if ($Destination -eq $inst -and $LiteralPath -like '*.new') { throw 'simulated failure' }; Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination }
$threw = $false; try { Install-Build $z4 } catch { $threw = $true }
Remove-Item Function:\Move-Item
Check ($threw -and (Get-Content (Join-Path $inst 'PhysicsLab.exe')) -eq 'v2') 'failed swap restores v2'
Check (-not (Test-Path "$inst.old") -and -not (Test-Path "$inst.new")) 'no .old/.new after rollback'
# patterns / partners
Check ('PhysicsLab-windows-x64 (12).zip' -like $ZipPattern -and 'PhysicsLab-AutoInstall.zip' -notlike $ZipPattern -and 'other.zip' -notlike $ZipPattern) 'zip pattern'
Set-Content "$z4.crdownload" 'x'
Check (Test-Partner $z4) '.crdownload partner seen'
Remove-Item "$z4.crdownload"
Check (-not (Test-Partner $z4)) 'no partner'
Remove-Item $z3, $z4

# watcher loop: old zip ignored, new zip installed and deleted
Get-ChildItem $dl | Remove-Item
$old = Make-Zip 'PhysicsLab-windows-x64 (9).zip' 'flat' 'old-ignored'
$pwsh = (Get-Process -Id $PID).Path
$p = Start-Process $pwsh -PassThru -ArgumentList @('-NoProfile', '-File', (Join-Path $tools 'PhysicsLabAutoInstall.ps1'), '-WatchDir', $dl, '-InstallDir', $inst, '-PollSeconds', '1', '-NoStart')
Start-Sleep 4
$new = Make-Zip 'PhysicsLab-windows-x64 (10).zip' 'flat' 'v10'
$p2 = Start-Process $pwsh -PassThru -ArgumentList @('-NoProfile', '-File', (Join-Path $tools 'PhysicsLabAutoInstall.ps1'), '-WatchDir', $dl, '-InstallDir', $inst, '-PollSeconds', '1', '-NoStart')   # 2nd instance must exit (mutex)
for ($i = 0; $i -lt 20 -and (Test-Path $new); $i++) { Start-Sleep 1 }
Check ((Get-Content (Join-Path $inst 'PhysicsLab.exe')) -eq 'v10') 'watcher installed the new zip'
Check (-not (Test-Path $new)) 'watcher deleted the installed zip'
Check (Test-Path $old) 'zip present at start ignored'
Check ($p2.WaitForExit(5000)) 'second instance exits (mutex)'
Check ((Get-Content (Join-Path $inst 'autoinstall.log') -Raw) -match 'OK installed') 'log line written'
Stop-Process -Id $p.Id -Force

# CleanOld: list only, nothing deleted
Set-Content (Join-Path $dl 'keep.zip') 'k'
New-Item -ItemType Directory -Path (Join-Path $dl 'PhysicsLab-windows-x64 (5)') | Out-Null
$out = & (Join-Path $tools 'CleanOld.ps1') -DownloadsDir $dl
$lst = Get-Content (Join-Path $dl 'PhysicsLab-cleanup-list.txt')
Check ($out -match '^2 items' -and $lst.Count -eq 2 -and (Test-Path $old) -and (Test-Path (Join-Path $dl 'keep.zip'))) 'CleanOld lists 2, deletes nothing'

Remove-Item $root -Recurse -Force
"fails: $fails"
exit $fails
