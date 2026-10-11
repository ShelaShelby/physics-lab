# Old Physics Lab downloads in Downloads (PhysicsLab-windows-x64* zips and folders).
# Default: only LISTS them (Downloads\PhysicsLab-cleanup-list.txt). With -Confirm: moves exactly the listed ones to the Recycle Bin.
param(
    [switch]$Confirm,
    [string]$DownloadsDir = ''
)
$ErrorActionPreference = 'Stop'
$Pattern = 'PhysicsLab-windows-x64*'

if (-not $DownloadsDir) {
    if ([Environment]::OSVersion.Platform -eq 'Win32NT') {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PlKnownFolder2 {
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
        $DownloadsDir = [PlKnownFolder2]::Downloads()
    } else { $DownloadsDir = Join-Path $HOME 'Downloads' }
}

function Get-Matches {
    # zips and folders only, and only in Downloads itself
    foreach ($i in @(Get-ChildItem -LiteralPath $DownloadsDir -Filter $Pattern -Force)) {
        if ($i.PSIsContainer -or $i.Extension -eq '.zip') { $i }
    }
}
function Get-ItemSize($i) {
    if (-not $i.PSIsContainer) { return $i.Length }
    $s = (Get-ChildItem -LiteralPath $i.FullName -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
    if ($null -eq $s) { return 0 } else { return $s }
}

$listFile = Join-Path $DownloadsDir 'PhysicsLab-cleanup-list.txt'

if (-not $Confirm) {
    $items = @(Get-Matches | Sort-Object LastWriteTime)
    $total = 0
    $lines = foreach ($i in $items) {
        $sz = Get-ItemSize $i
        $total += $sz
        '{0}  {1,12:N0} bytes  {2}{3}' -f $i.LastWriteTime.ToString('yyyy-MM-dd HH:mm'), $sz, $i.Name, $(if ($i.PSIsContainer) { '  [folder]' } else { '' })
    }
    @($lines) | Set-Content -LiteralPath $listFile -Encoding UTF8
    '{0} items, {1:N1} MB. List: {2}. Nothing deleted. To move them to the Recycle Bin: CleanOld.ps1 -Confirm' -f $items.Count, ($total / 1MB), $listFile
    return
}

if (-not (Test-Path -LiteralPath $listFile)) { throw 'Run CleanOld.ps1 without -Confirm first, so there is a list to check.' }
$listed = @(Get-Content -LiteralPath $listFile -Encoding UTF8)
Add-Type -AssemblyName Microsoft.VisualBasic
$n = 0
foreach ($i in @(Get-Matches)) {
    # exactly the listed items: the list line must contain the name as its last field (a folder line ends with "[folder]")
    $hit = $listed | Where-Object { $_.EndsWith('  ' + $i.Name) -or $_.EndsWith('  ' + $i.Name + '  [folder]') }
    if (-not $hit) { continue }
    if ($i.PSIsContainer) {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($i.FullName, 'OnlyErrorDialogs', 'SendToRecycleBin')
    } else {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($i.FullName, 'OnlyErrorDialogs', 'SendToRecycleBin')
    }
    $n++
}
"$n items moved to the Recycle Bin."
