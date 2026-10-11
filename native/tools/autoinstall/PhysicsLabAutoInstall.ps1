# Physics Lab auto-installer: watches Downloads for a new PhysicsLab-windows-x64*.zip and installs it into C:\PhysicsLab.
# Settings of the app live in %APPDATA%\PhysicsLab\PhysicsLab\settings.txt (SDL_GetPrefPath, native/src/ui/settings.cpp),
# not next to the exe, so a replace of the app folder cannot lose them. Only autoinstall.log is carried over.
# -NoRun only defines the functions (used by the Linux tests); -WatchDir / -InstallDir / -NoStart are for tests too.
param(
    [string]$WatchDir = '',
    [string]$InstallDir = 'C:\PhysicsLab',
    [string]$ExeName = 'PhysicsLab.exe',
    [int]$PollSeconds = 3,
    [switch]$NoStart,
    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
$ZipPattern = 'PhysicsLab-windows-x64*.zip'

function Get-DownloadsPath {
    # Windows known-folder API (FOLDERID_Downloads), not a hardcoded string.
    if ([Environment]::OSVersion.Platform -ne 'Win32NT') { return (Join-Path $HOME 'Downloads') }
    if (-not ('PlKnownFolder' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PlKnownFolder {
    [DllImport("shell32.dll")]
    private static extern int SHGetKnownFolderPath([MarshalAs(UnmanagedType.LPStruct)] Guid rfid, uint dwFlags, IntPtr hToken, out IntPtr ppszPath);
    public static string Downloads() {
        IntPtr p;
        int hr = SHGetKnownFolderPath(new Guid("374DE290-123F-4565-9164-39C4925E467B"), 0, IntPtr.Zero, out p);
        if (hr != 0) throw new System.ComponentModel.Win32Exception(hr);
        string s = Marshal.PtrToStringUni(p);
        Marshal.FreeCoTaskMem(p);
        return s;
    }
}
'@
    }
    return [PlKnownFolder]::Downloads()
}

function Write-InstallLog([string]$Zip, [string]$Result) {
    try {
        if (-not (Test-Path -LiteralPath $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null }
        $line = '{0}  {1}  {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Zip, $Result
        Add-Content -LiteralPath (Join-Path $InstallDir 'autoinstall.log') -Value $line -Encoding UTF8
    } catch { }   # a log problem must never stop the watcher
}

function Test-Partner([string]$Path) {
    # a browser still writing: name.zip.crdownload / .part / .tmp (or the same without ".zip")
    $base = $Path -replace '\.zip$', ''
    foreach ($p in @($Path, $base)) {
        foreach ($e in '.crdownload', '.part', '.tmp') { if (Test-Path -LiteralPath ($p + $e)) { return $true } }
    }
    return $false
}

function Test-ExclusiveRead([string]$Path) {
    try {
        $fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
        $fs.Dispose()
        return $true
    } catch { return $false }
}

function Stop-RunningApp {
    # Only an exe that runs from the install folder is closed.
    $root = [IO.Path]::GetFullPath($InstallDir).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $name = [IO.Path]::GetFileNameWithoutExtension($ExeName)
    foreach ($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
        $path = $null
        try { $path = $p.Path } catch { }
        if (-not $path -or -not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { continue }
        [void]$p.CloseMainWindow()
        if (-not $p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; [void]$p.WaitForExit(5000) }
    }
}

function Install-Build([string]$ZipPath) {
    # Extract next to the install folder (same drive, so the moves are instant), check, swap, roll back on any failure.
    $new = $InstallDir + '.new'
    $old = $InstallDir + '.old'
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ('PhysicsLab-ai-' + [guid]::NewGuid().ToString('N'))
    $swapped = $false
    $movedOld = $false
    try {
        New-Item -ItemType Directory -Path $tmp | Out-Null
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $tmp -Force
        $top = @(Get-ChildItem -LiteralPath $tmp -Force)
        $root = $tmp
        if ($top.Count -eq 1 -and $top[0].PSIsContainer) { $root = $top[0].FullName }   # zip with one top-level folder
        if (-not (Test-Path -LiteralPath (Join-Path $root $ExeName))) { throw "$ExeName not found in the zip" }

        if (Test-Path -LiteralPath $new) { Remove-Item -LiteralPath $new -Recurse -Force }
        Move-Item -LiteralPath $root -Destination $new
        $oldLog = Join-Path $InstallDir 'autoinstall.log'
        if (Test-Path -LiteralPath $oldLog) { Copy-Item -LiteralPath $oldLog -Destination (Join-Path $new 'autoinstall.log') -Force }

        Stop-RunningApp
        if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }   # leftover of an earlier run
        if (Test-Path -LiteralPath $InstallDir) { Move-Item -LiteralPath $InstallDir -Destination $old; $movedOld = $true }
        Move-Item -LiteralPath $new -Destination $InstallDir
        $swapped = $true
        if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force -ErrorAction SilentlyContinue }   # only after the new one is in place
    } catch {
        if (-not $swapped -and $movedOld -and (Test-Path -LiteralPath $old)) {
            if (Test-Path -LiteralPath $InstallDir) { Remove-Item -LiteralPath $InstallDir -Recurse -Force -ErrorAction SilentlyContinue }
            Move-Item -LiteralPath $old -Destination $InstallDir -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $new) { Remove-Item -LiteralPath $new -Recurse -Force -ErrorAction SilentlyContinue }
        throw
    } finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

function Invoke-Zip([string]$ZipPath) {
    $name = Split-Path -Leaf $ZipPath
    try {
        Install-Build $ZipPath
        if (-not $NoStart) { Start-Process -FilePath (Join-Path $InstallDir $ExeName) -WorkingDirectory $InstallDir }
        Remove-Item -LiteralPath $ZipPath -Force
        Write-InstallLog $name 'OK installed'
    } catch {
        Write-InstallLog $name ('ERROR ' + $_.Exception.Message + ' (zip left in place)')
    }
}

function Get-Snapshot([string]$Dir) {
    $h = @{}
    foreach ($f in @(Get-ChildItem -LiteralPath $Dir -Filter $ZipPattern -File -ErrorAction SilentlyContinue)) {
        $h[$f.Name] = '{0}|{1}' -f $f.Length, $f.LastWriteTimeUtc.Ticks
    }
    return $h
}

function Start-Watcher {
    if (-not $WatchDir) { $script:WatchDir = Get-DownloadsPath }
    $mutex = New-Object System.Threading.Mutex($false, 'Local\PhysicsLabAutoInstall')
    if (-not $mutex.WaitOne(0)) { return }   # already running
    try {
        $ignore = Get-Snapshot $WatchDir      # zips present at start (name|size|time) are never touched
        $lastSize = @{}
        $given = @{}                           # failed ones are tried once per version of the file
        while ($true) {
            try {
                $now = Get-Snapshot $WatchDir
                foreach ($n in @($now.Keys)) {
                    if ($ignore[$n] -eq $now[$n] -or $given[$n] -eq $now[$n]) { continue }
                    $path = Join-Path $WatchDir $n
                    $size = $now[$n].Split('|')[0]
                    $stable = ($lastSize[$n] -eq $size)
                    $lastSize[$n] = $size
                    if (-not $stable) { continue }                 # size must be unchanged over two polls
                    if (Test-Partner $path) { continue }
                    if (-not (Test-ExclusiveRead $path)) { continue }
                    $given[$n] = $now[$n]
                    Invoke-Zip $path
                }
            } catch { Write-InstallLog '-' ('WATCHER ERROR ' + $_.Exception.Message) }
            Start-Sleep -Seconds $PollSeconds
        }
    } finally { $mutex.ReleaseMutex(); $mutex.Dispose() }
}

if (-not $NoRun) { Start-Watcher }
