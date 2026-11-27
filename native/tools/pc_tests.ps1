# pc_tests.ps1 (0.85.1): the checks that need real hardware, run on the owner's PC from one folder.
# Needs only Windows PowerShell 5.1 and the .NET Framework that comes with Windows. Installs nothing.
#
# Put PhysicsLab.exe (from PhysicsLab-windows-x64.zip) and the files of PhysicsLab-tests-windows-x64.zip in one folder, then:
#   powershell -ExecutionPolicy Bypass -File pc_tests.ps1                 (every part)
#   powershell -ExecutionPolicy Bypass -Command "& ./pc_tests.ps1 -Parts smoke,bh"   (some parts)
# Parts: device, smoke, vkcards, bh, planets, bench, items, logs, validation. Default: all of them, in that order.
# Writes results\pc-results.txt (one line per check: PASS, FAIL or INFO) and each run's output beside it.
# The app runs with its own settings file in results\ (PL_SETTINGS), so the owner's settings are never read or changed.
# No single app start runs longer than 14 minutes. The benchmark has its own limit (20 minutes here).
param(
    [string]$Folder = $PSScriptRoot,
    [string[]]$Parts = @('device', 'smoke', 'vkcards', 'bh', 'planets', 'bench', 'items', 'logs', 'validation'),
    [int]$Width = 1280,
    [int]$Height = 800,
    [string]$WindowAt = '',     # "x,y": where the app's windows open (another monitor, so the owner can keep using his main one); default: centred
    [switch]$NoKeys              # skip the full-screen check (it presses F11 through Windows and takes the focus)
)
$ErrorActionPreference = 'Stop'
$Parts = @($Parts | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })   # (-File passes "a,b" as one string)
$inv = [System.Globalization.CultureInfo]::InvariantCulture
$app = Join-Path $Folder 'PhysicsLab.exe'
$res = Join-Path $Folder 'results'
New-Item -ItemType Directory -Force $res | Out-Null
$summary = Join-Path $res 'pc-results.txt'
$script:nPass = 0; $script:nFail = 0

function Write-Line([string]$s) { Add-Content -Path $summary -Value $s -Encoding UTF8; Write-Host $s }
function Add-Check([string]$id, [bool]$ok, [string]$what) {
    if ($ok) { $script:nPass++; Write-Line "PASS $id  $what" } else { $script:nFail++; Write-Line "FAIL $id  $what" }
}
function Add-Info([string]$id, [string]$what) { Write-Line "INFO $id  $what" }

# ---- the picture helper: BMP reading and the comparisons (compiled from this text by the .NET Framework, no install) ----
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Collections.Generic;
public class PlBmp {
    public int W, H, Bpp, Stride, Off; public bool BottomUp; public byte[] D;
    public PlBmp(string path) {
        D = File.ReadAllBytes(path);
        Off = BitConverter.ToInt32(D, 10); W = BitConverter.ToInt32(D, 18); int h = BitConverter.ToInt32(D, 22);
        Bpp = BitConverter.ToInt16(D, 28) / 8; Stride = (W * Bpp + 3) / 4 * 4; BottomUp = h > 0; H = Math.Abs(h);
    }
    public int Px(int x, int y, int c) {   // c: 0 R, 1 G, 2 B
        int row = BottomUp ? (H - 1 - y) : y;
        return D[Off + row * Stride + x * Bpp + (2 - c)];
    }
    // ui_smoke's test (check_shell.py part_smoke): the middle of the picture, every second pixel; colours and samples away from the main colour
    public static int[] Smoke(string path) {
        PlBmp b = new PlBmp(path);
        Dictionary<int, int> counts = new Dictionary<int, int>(); List<int> px = new List<int>();
        for (int y = (int)(0.3 * b.H); y < (int)(0.7 * b.H); y += 2)
            for (int x = (int)(0.25 * b.W); x < (int)(0.6 * b.W); x += 2) {
                int c = (b.Px(x, y, 0) << 16) | (b.Px(x, y, 1) << 8) | b.Px(x, y, 2);
                px.Add(c); int n; counts.TryGetValue(c, out n); counts[c] = n + 1;
            }
        int mode = 0, best = -1;
        foreach (KeyValuePair<int, int> kv in counts) if (kv.Value > best) { best = kv.Value; mode = kv.Key; }
        int off = 0;
        foreach (int c in px) {
            int d = Math.Max(Math.Abs(((c >> 16) & 255) - ((mode >> 16) & 255)), Math.Max(Math.Abs(((c >> 8) & 255) - ((mode >> 8) & 255)), Math.Abs((c & 255) - (mode & 255))));
            if (d > 24) off++;
        }
        return new int[] { counts.Count, off, px.Count };
    }
    // check_orbit_ui.py compare_cpu: the GPU's picture against planet_picture's (a text header "x w h x0 y0", then RGBA); pixels over 8 levels in a channel
    public static int[] CompareCpu(string bin, string bmp) {
        byte[] d = File.ReadAllBytes(bin);
        int nl = Array.IndexOf(d, (byte)10);
        string[] head = System.Text.Encoding.ASCII.GetString(d, 0, nl).Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        int w = int.Parse(head[1]), h = int.Parse(head[2]), x0 = int.Parse(head[3]), y0 = int.Parse(head[4]);
        PlBmp b = new PlBmp(bmp);
        int n = 0, bad = 0, worst = 0;
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
            int o = nl + 1 + 4 * (j * w + i);
            if (d[o + 3] == 0) continue;
            int x = x0 + i, y = y0 + j;
            if (x < 0 || y < 0 || x >= b.W || y >= b.H) continue;
            int m = 0;
            for (int c = 0; c < 3; c++) m = Math.Max(m, Math.Abs(b.Px(x, y, c) - d[o + c]));
            n++; if (m > worst) worst = m; if (m > 8) bad++;
        }
        return new int[] { n, bad, worst };
    }
}
'@

# ---- one start of a program, with a time limit; stdout and stderr to files ----
function Invoke-Tool([string]$exe, [string[]]$argList, [string]$name, [hashtable]$envs, [int]$limitS, [string]$cwd, [string]$stopOn = $null) {
    $outF = Join-Path $res "$name.out.txt"; $errF = Join-Path $res "$name.err.txt"
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = ($argList | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
    $psi.UseShellExecute = $false; $psi.RedirectStandardOutput = $true; $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $(if ($cwd) { $cwd } else { $res })
    foreach ($k in @($psi.EnvironmentVariables.Keys)) { if ($k -like 'PL_*') { $psi.EnvironmentVariables.Remove($k) } }
    if ($envs) { foreach ($k in $envs.Keys) { $psi.EnvironmentVariables[$k] = [string]$envs[$k] } }
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    $t0 = Get-Date
    $sbo = New-Object System.Text.StringBuilder; $sbe = New-Object System.Text.StringBuilder
    $eo = Register-ObjectEvent $p OutputDataReceived -MessageData $sbo -Action { if ($null -ne $EventArgs.Data) { [void]$Event.MessageData.AppendLine($EventArgs.Data) } }
    $ee = Register-ObjectEvent $p ErrorDataReceived -MessageData $sbe -Action { if ($null -ne $EventArgs.Data) { [void]$Event.MessageData.AppendLine($EventArgs.Data) } }
    [void]$p.Start(); $p.BeginOutputReadLine(); $p.BeginErrorReadLine()
    # stopOn: a file pattern in the working folder; the app is closed 5 s after such a file appears (the benchmark: its saved result)
    $done = $false; $deadline = (Get-Date).AddSeconds($limitS)
    while ((Get-Date) -lt $deadline) {
        if ($p.WaitForExit(1000)) { $done = $true; break }
        if ($stopOn -and (Get-ChildItem $psi.WorkingDirectory -Filter $stopOn -ErrorAction SilentlyContinue)) { Start-Sleep 5; if (-not $p.HasExited) { [void]$p.CloseMainWindow(); if (-not $p.WaitForExit(15000)) { try { $p.Kill() } catch {} } }; $done = $true; break }
    }
    if (-not $done) { try { $p.Kill() } catch {} }
    $p.WaitForExit(10000) | Out-Null; $p.WaitForExit()
    Unregister-Event -SourceIdentifier $eo.Name; Unregister-Event -SourceIdentifier $ee.Name
    $out = $sbo.ToString(); $err = $sbe.ToString()
    [System.IO.File]::WriteAllText($outF, $out); [System.IO.File]::WriteAllText($errF, $err)
    return [pscustomobject]@{ Out = $out; Err = $err; Code = $(if ($done) { $p.ExitCode } else { $null }); TimedOut = -not $done; Seconds = ((Get-Date) - $t0).TotalSeconds }
}

# the app, as tests/shell_lib.py starts it (its own settings file, a fixed frame time, the screenshot of the last frame), on the real window and driver
function Invoke-App([string]$name, [hashtable]$extra, [int]$frames = 60, [int]$limitS = 840) {
    $hd = Join-Path $res "home_$name"
    if (Test-Path $hd) { Remove-Item -Recurse -Force $hd }
    New-Item -ItemType Directory -Force $hd | Out-Null
    $bmp = Join-Path $res "$name.bmp"
    if ($WindowAt) { Set-Content -Path (Join-Path $hd 'settings.txt') -Value "window=$Width,$Height,$WindowAt,0" -Encoding ASCII }
    if (Test-Path $bmp) { Remove-Item -Force $bmp }
    $e = @{ PL_SETTINGS = (Join-Path $hd 'settings.txt'); PL_SHOT_DIR = $hd; PL_WINDOW = "${Width}x${Height}"; PL_MOTION = 'on'; PL_FIXED_DT = '0.016';
            PL_DYK_FIRST = '3'; PL_DATE = '2026-10-05'; PL_EXIT_AFTER_FRAMES = "$frames"; PL_THEME = 'dark'; PL_LOG_SHELL = '1'; PL_PANELS = 'atom.card=o';
            PL_NO_GPU_HELPER = '1'; PL_SCREENSHOT = $bmp }
    if ($extra) { foreach ($k in $extra.Keys) { if ($null -eq $extra[$k]) { $e.Remove($k) } else { $e[$k] = $extra[$k] } } }
    $r = Invoke-Tool $app @() $name $e $limitS $hd
    $r | Add-Member Bmp $bmp
    $r | Add-Member Home $hd
    $r | Add-Member Log (Join-Path $hd 'gpu-paths-log.txt')
    return $r
}

function Get-Rows([string]$text, [string]$tag) {   # "TAG k=v k=v" lines as hashtables
    $rows = @()
    foreach ($ln in ($text -split "`r?`n")) {
        if ($ln.StartsWith("$tag ")) {
            $h = @{}
            foreach ($kv in ($ln.Substring($tag.Length + 1) -split '\s+')) { $i = $kv.IndexOf('='); if ($i -gt 0) { $h[$kv.Substring(0, $i)] = $kv.Substring($i + 1) } }
            $rows += , $h
        }
    }
    return , $rows
}
function N([string]$s) { return [double]::Parse($s, $inv) }

Set-Content -Path $summary -Value "Physics Lab hardware checks, $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -Encoding UTF8
if (-not (Test-Path $app)) { Add-Check 'setup' $false "PhysicsLab.exe is not in $Folder"; exit 1 }
Add-Info 'setup' ("PhysicsLab.exe " + (Get-Item $app).VersionInfo.ProductVersion + ", " + (Get-Item $app).LastWriteTime.ToString('yyyy-MM-dd HH:mm'))

# ---- 3a: the device ----
if ($Parts -contains 'device') {
    foreach ($g in (Get-CimInstance Win32_VideoController)) {
        Add-Info 'device' ("Windows: $($g.Name), driver $($g.DriverVersion), " + $(if ($g.CurrentRefreshRate) { "$($g.CurrentHorizontalResolution) x $($g.CurrentVerticalResolution) at $($g.CurrentRefreshRate) Hz" } else { 'no screen' }))
    }
    $vi = Join-Path $env:WINDIR 'System32\vulkaninfo.exe'
    if (Test-Path $vi) {
        $r = Invoke-Tool $vi @('--summary') 'vulkaninfo' @{} 120 $res
        foreach ($ln in ($r.Out -split "`r?`n")) { if ($ln -match '^\s*(deviceName|deviceType|apiVersion|driverVersion|driverName|driverInfo)\s*=') { Add-Info 'device' ("vulkaninfo: " + $ln.Trim()) } }
    } else { Add-Info 'device' 'vulkaninfo.exe not found' }
    try { $py = (& python --version) 2>&1 } catch { $py = 'none' }
    Add-Info 'device' ("python --version: $py")
}

# ---- 3b: start, each lab draws, clean exit (ui_smoke, without the validation layers unless installed) ----
$labs = [ordered]@{ orbit = @{}; atom = @{ PL_ATOM = '1' }; molecule = @{ PL_MOLECULE = 'H2O' }; blackhole = @{ PL_BLACKHOLE = '1' } }
if ($Parts -contains 'smoke') {
    foreach ($lab in $labs.Keys) {
        $r = Invoke-App "smoke_$lab" $labs[$lab] 30 300
        $vk = ($r.Out -split "`r?`n" | Where-Object { $_.StartsWith('Vulkan: ') } | Select-Object -First 1)
        Add-Check "3b.smoke.$lab.start" ($r.Code -eq 0 -and $vk) "the app started on Vulkan and ended with exit code 0 (exit $($r.Code), $([math]::Round($r.Seconds,1)) s): $vk"
        if (Test-Path $r.Bmp) {
            $s = [PlBmp]::Smoke($r.Bmp)
            Add-Check "3b.smoke.$lab.draws" ($s[0] -ge 20 -and $s[1] -ge 0.08 * $s[2]) "the frame is not empty ($($s[0]) colours, $($s[1]) of $($s[2]) samples away from the main colour)"
        } else { Add-Check "3b.smoke.$lab.draws" $false 'the frame was read back (no picture saved)' }
    }
}

# ---- 3c: the GPU against the CPU: the card programs and the renderer's pictures (vk_compare_tests) ----
if ($Parts -contains 'vkcards') {
    $vc = Join-Path $Folder 'vk_compare_tests.exe'
    if (Test-Path $vc) {
        $pics = Join-Path $res 'vk_pictures'; New-Item -ItemType Directory -Force $pics | Out-Null
        $r = Invoke-Tool $vc @($pics) 'vk_compare_tests' @{} 840 $res
        $dev = ($r.Out -split "`r?`n" | Where-Object { $_.StartsWith('Vulkan: ') } | Select-Object -First 1)
        $sum = ($r.Out -split "`r?`n" | Where-Object { $_ -match '^\d+ Vulkan checks, \d+ failed' } | Select-Object -Last 1)
        Add-Info '3c.vkcards' "$dev"
        foreach ($ln in ($r.Out -split "`r?`n" | Where-Object { $_ -match '^\s*FAIL' })) { Add-Info '3c.vkcards' $ln.Trim() }
        # 0.85.2: the tests print "PART <name>" before each part, unbuffered: the last one is the part it ended in (the one it crashed in, if it crashed)
        $lastPart = ($r.Out -split "`r?`n" | Where-Object { $_.StartsWith('PART ') } | Select-Object -Last 1)
        if ($lastPart) { Add-Info '3c.vkcards' "last part begun: $($lastPart.Substring(5))" }
        Add-Check '3c.vkcards' ($r.Code -eq 0 -and $sum -match ', 0 failed') "vk_compare_tests: $sum (exit $($r.Code), $([math]::Round($r.Seconds,1)) s)"
    } else { Add-Check '3c.vkcards' $false 'vk_compare_tests.exe is not in the folder' }
}

# ---- 3c, 3e, 3f: the Black Hole Lab's card against its CPU references (check_blackhole.py sky, precision, edgeon) ----
$GL = @{ PL_NEWUI = 'on'; PL_DRAWSTAT = '1'; PL_FACTS = 'off'; PL_BH_GAS = 'off'; PL_BLACKHOLE = '1' }
function Merge($a, $b) { $h = @{}; foreach ($k in $a.Keys) { $h[$k] = $a[$k] }; foreach ($k in $b.Keys) { $h[$k] = $b[$k] }; return $h }
if ($Parts -contains 'bh') {
    # the camera curve (PLBHTONE), with close, closespin, pole and closetable (0.84.0), and the sky at the owner's window size
    $r = Invoke-App 'bh_sky' (Merge $GL @{ PL_BH_COMPARE = 'sky' }) 1 840
    Add-Check '3c.bh.sky.run' ($r.Code -eq 0) "the sky comparison ran (exit $($r.Code), $([math]::Round($r.Seconds,1)) s)"
    $c = Get-Rows $r.Out 'PLBHSKYCHECK'
    Add-Check '3c.bh.sky.startup' ($c.Count -ge 1 -and $c[0]['ok'] -eq '1') "the sky's start-up check passes on this driver"
    $tn = Get-Rows $r.Out 'PLBHSKYCHECKTONE'
    Add-Check '3c.bh.sky.tonecheck' ($tn.Count -ge 1 -and $tn[0]['bad'] -eq '0') "the start-up check's camera curve: every probe pixel within its limit ($(if ($tn.Count) { $tn[0]['bad'] + ' of ' + $tn[0]['px'] + ' over' }))"
    foreach ($x in (Get-Rows $r.Out 'PLBHTONE')) {
        $floor = $(if ($x['case'] -eq 'close') { 2000 } else { 50000 })
        Add-Check "3c.bh.tone.$($x['case'])" ($x['ok'] -eq '1' -and [int]$x['pixels'] -gt $floor -and (N $x['share']) -le 0.01) "the camera curve's picture: $($x['over8']) of $($x['pixels']) disc pixels over 8 levels (max $($x['maxdiff']))"
    }
    foreach ($x in (Get-Rows $r.Out 'PLBHSKYPIC')) {
        $kvs = ($x.Keys | Sort-Object | ForEach-Object { "$_=$($x[$_])" }) -join ' '
        Add-Info "3c.bh.skypic.$($x['case'])" $kvs
    }
    # the full picture at the owner's window (2560 x 1369) against the double-precision reference, at four tilts
    $r = Invoke-App 'bh_skyfull' (Merge $GL @{ PL_BH_COMPARE = 'sky'; PL_BH_SKYFULL = '2560,1369,30' }) 1 840
    $full = Get-Rows $r.Out 'PLBHSKYFULL'
    Add-Check '3c.bh.skyfull.count' ($full.Count -eq 4) "Film at 2560 x 1369 compared at four tilts ($($full.Count); exit $($r.Code), $([math]::Round($r.Seconds,1)) s)"
    foreach ($x in $full) {
        $bad = @('lost', 'reflost', 'endsfar', 'dirbad', 'discwrongout', 'rbad', 'ubad') | Where-Object { $x[$_] -and [int]$x[$_] -ne 0 } | ForEach-Object { "$_=$($x[$_])" }
        Add-Check "3c.bh.skyfull.$($x['case'])" (-not $bad) "every pixel within the limits ($(if ($bad) { $bad -join ' ' } else { 'none over' }); dir $($x['dir']), r $($x['r']), u $($x['u']))"
    }
    # the shadow's edge and the bent sky at three distances (PLBHCMP)
    $r = Invoke-App 'bh_precision' (Merge $GL @{ PL_BH_COMPARE = '1' }) 3 840
    foreach ($d in (Get-Rows $r.Out 'PLBHCMP')) {
        Add-Check "3c.bh.precision.$($d['dist'])" ((N $d['edgeerr']) -lt 1e-3 -and [int]$d['wrong'] -eq 0 -and (N $d['dir1']) -lt 1e-5 -and (N $d['dir01']) -lt 1e-5) "the shadow's edge $($d['edgeerr']) px, $($d['wrong']) on the wrong side, the bent sky $($d['dir1']) rad, from 0.1 px of the edge $($d['dir01']) rad (limits 1e-3 px, 1e-5 rad)"
    }
    # 3e: the static near the disc's plane, pitch near 90 degrees (check_blackhole.py part_edgeon, the views the owner saw)
    $views = New-Object System.Collections.ArrayList
    [void]$views.Add(@(57.5, 1.575)); [void]$views.Add(@(1000, 1.575)); [void]$views.Add(@(1000, ([math]::PI / 2))); [void]$views.Add(@(57.5, ([math]::PI / 2)))
    foreach ($dd in @(57.5, 1000)) { foreach ($o in @(-0.0035, -0.0018, -0.0007, 0.0007, 0.0018, 0.0035)) { [void]$views.Add(@($dd, ([math]::PI / 2 + $o))) } }
    $spec = ($views | ForEach-Object { [string]::Format($inv, '{0},{1:F7},0.11', $_[0], $_[1]) }) -join ';'
    $r = Invoke-App 'bh_edgeon' (Merge $GL @{ PL_BH_COMPARE = '1'; PL_BH_COMPARE_VIEWS = $spec }) 3 840
    $rows = Get-Rows $r.Out 'PLBHEDGEON'
    Add-Check '3e.edgeon.count' ($rows.Count -eq $views.Count) "the edge-on views were compared ($($rows.Count) of $($views.Count); exit $($r.Code))"
    foreach ($x in $rows) {
        $n = [int]$x['pixels']; $sm = [int]$x['smooth']
        Add-Check "3e.edgeon.$($x['dist'])@$($x['pitch'])" ([int]$x['bad'] -le 0.001 * $n -and [int]$x['badorder'] -le 0.001 * $n -and $sm -gt 100000 -and [int]$x['over8'] -le 0.01 * $sm) "the card = the double lookup ($($x['bad']) of $n differ), the near disc smooth ($($x['over8']) of $sm over 8 levels)"
    }    # 0.95.0: the spinning hole's sky pass alone at the owner's window (GPU timestamps, 9 pictures each), whole and cut after its set-up (1), its radial case
    # (2), its integrals and crossings (3), its whole ray (4), at once (5): the split of REPORT 0.95.0 on this card (blackhole_view.cpp skyTimeHook)
    foreach ($k in 0..5) {
        $e = @{ PL_BH_COMPARE = 'skytime'; PL_BH_SKYTIME = '2560,1369,9' }
        if ($k -gt 0) { $e['PL_BH_SKYSTOP'] = "$k" }
        $r = Invoke-App "bh_skytime$k" (Merge $GL $e) 3 840
        $rows = Get-Rows $r.Out 'PLBHSKYTIME'
        Add-Check "3c.bh.skytime.$k" ($rows.Count -ge 3) "the sky pass timed, cut at $k ($($rows.Count) views; exit $($r.Code))"
        foreach ($x in $rows) { Add-Info "3c.bh.skytime.$k.$($x['case'])" "median $($x['median']) ms, fastest $($x['min']) ms ($($x['w']) x $($x['h']), moving gas $($x['gas']))" }
    }
}

# ---- 3c, 3f: the Orbit Lab's true sunlight, eclipse and the Sun's edge against the CPU twin planet_picture (check_orbit_ui.py review085); 0.94.0: NASA's photo downloaded and drawn ----
if ($Parts -contains 'planets') {
    $tool = Join-Path $Folder 'planet_picture.exe'
    if (-not (Test-Path $tool)) { Add-Check '3c.planets' $false 'planet_picture.exe is not in the folder' }
    else {
        $want = @{ Jupiter = 5; Earth = 2; Neptune = 8 }
        $cases = @(@('Earth', '4.5', @{ PL_TRUELIGHT = '1' }, 'true_Earth'), @('Jupiter', '3.4', @{ PL_TRUELIGHT = '1' }, 'true_Jupiter'), @('Neptune', '3.9', @{ PL_TRUELIGHT = '1' }, 'true_Neptune'),
                   @('Earth', '4.5', @{ PL_DATE = '2024-04-08'; PL_TIME = '18:18'; PL_PAUSED = '1' }, 'eclipse_Earth'))
        foreach ($cs in $cases) {
            $sel, $zoom, $extra, $tag = $cs
            $e = Merge @{ PL_NEWUI = 'on'; PL_SELECT = $sel; PL_ZOOM = $zoom; PL_LOG_PLANET = '1'; PL_LOG_BODYPX = '1'; PL_SKYOFF = '1' } $extra
            $r = Invoke-App "planets_$tag" $e 40 300
            $lines = $r.Out -split "`r?`n"
            $rows = @()
            for ($i = 0; $i -lt $lines.Count - 1; $i++) {
                if ($lines[$i].StartsWith('PLPLANET ') -and $lines[$i + 1].StartsWith('PLPLANETX')) { $rows += , (@($lines[$i] -split '\s+') + @(($lines[$i + 1] -split '\s+') | Select-Object -Skip 1)) }
            }
            $b = $null; if (Test-Path $r.Bmp) { $b = New-Object PlBmp $r.Bmp }
            $mine = @($rows | Select-Object -Skip ([int]($rows.Count * 3 / 4)) | Where-Object { [int]$_[1] -eq $want[$sel] -and $b -and (N $_[2]) -ge 0 -and (N $_[2]) -lt $b.W -and (N $_[3]) -ge 0 -and (N $_[3]) -lt $b.H } | Sort-Object { N $_[4] })
            if (-not $mine.Count -or -not $b) { Add-Check "3c.planets.$tag" $false "the app logged its ball and saved the picture ($($rows.Count) PLPLANET lines)"; continue }
            $f = @($mine[-1] | Select-Object -Skip 1)
            $bin = Join-Path $res "planet_cpu_$tag.bin"
            $p = Invoke-Tool $tool (@($bin) + $f) "planet_cpu_$tag" @{} 300 $res
            if ($p.Code -ne 0) { Add-Check "3c.planets.$tag" $false "planet_picture ran ($($p.Err.Trim()))"; continue }
            $c = [PlBmp]::CompareCpu($bin, $r.Bmp)
            Add-Check "3c.planets.$tag" ($c[0] -gt 5000 -and $c[1] -le 0.01 * $c[0]) "GPU against CPU: $($c[1]) of $($c[0]) pixels over 8 levels (at most 1 %; largest $($c[2])); radius $($f[3]) px"
        }
        $r = Invoke-App 'planets_sun' @{ PL_NEWUI = 'on'; PL_SELECT = 'Sun'; PL_ZOOM = '2.6'; PL_LOG_PLANET = '1'; PL_SKYOFF = '1'; PL_DATE = '2024-05-10'; PL_TIME = '00:00'; PL_PAUSED = '1'; PL_SUNPHOTO_OFFLINE = '1'; PL_SUNPHOTO_CACHE = (Join-Path $res 'sun_photos_empty') } 40 300
        $suns = @($r.Out -split "`r?`n" | Where-Object { $_.StartsWith('PLSUN ') })
        if ($suns.Count -and (Test-Path $r.Bmp)) {
            $f = @(($suns[-1] -split '\s+') | Select-Object -Skip 1)
            Add-Check '3f.sun_groups' ([int]$f[12] -eq 7) "the Sun on 10 May 2024: the record's 7 groups drawn ($($f[12]))"
            $bin = Join-Path $res 'planet_cpu_sun.bin'
            $p = Invoke-Tool $tool (@($bin, 'sun') + $f) 'planet_cpu_sun' @{} 300 $res
            if ($p.Code -eq 0) {
                $c = [PlBmp]::CompareCpu($bin, $r.Bmp)
                Add-Check '3c.planets.sun' ($c[0] -gt 5000 -and $c[1] -le 0.01 * $c[0]) "the Sun's edge and groups, GPU against CPU: $($c[1]) of $($c[0]) pixels over 8 levels (largest $($c[2]))"
            } else { Add-Check '3c.planets.sun' $false "planet_picture ran ($($p.Err.Trim()))" }
        } else { Add-Check '3c.planets.sun' $false "the app logged the Sun ($($suns.Count) PLSUN lines)" }
        # 0.94.0: NASA SDO/HMI's photo of 10 May 2024 downloaded (WinHTTP, on its worker) into a fresh cache, then the Sun with it: GPU (the Radeon) against CPU.
        # 4000 frames: the download (about 1.3 MB) and the picture's decoding finish within them; the check says when not
        $pc = Join-Path $res 'sun_photos'
        if (Test-Path $pc) { Remove-Item -Recurse -Force $pc }
        $r = Invoke-App 'planets_sunphoto' @{ PL_NEWUI = 'on'; PL_SELECT = 'Sun'; PL_ZOOM = '2.6'; PL_LOG_PLANET = '1'; PL_SKYOFF = '1'; PL_DATE = '2024-05-10'; PL_TIME = '00:00'; PL_PAUSED = '1'; PL_SUNPHOTO_CACHE = $pc } 4000 300
        $got = Test-Path (Join-Path $pc '2024-05-10.png')
        Add-Check '3f.sun_photo_download' $got "the photo of 10 May 2024 downloaded and saved ($(if ($got) { [math]::Round((Get-Item (Join-Path $pc '2024-05-10.png')).Length / 1MB, 2) } else { 0 }) MB)"
        $suns = @($r.Out -split "`r?`n" | Where-Object { $_.StartsWith('PLSUN ') })
        if ($suns.Count -and (Test-Path $r.Bmp)) {
            $f = @(($suns[-1] -split '\s+') | Select-Object -Skip 1)
            $n = [int]$f[12]
            $on = $f[13 + 6 * $n + 5]
            Add-Check '3f.sun_photo_shown' ($on -eq '1') "the last frame drew the photo ($on)"
            $bin = Join-Path $res 'planet_cpu_sunphoto.bin'
            $p = Invoke-Tool $tool (@($bin, 'sun') + $f) 'planet_cpu_sunphoto' @{} 300 $res
            if ($p.Code -eq 0) {
                $c = [PlBmp]::CompareCpu($bin, $r.Bmp)
                Add-Check '3c.planets.sunphoto' ($c[0] -gt 5000 -and $c[1] -le 0.01 * $c[0]) "the Sun with NASA's photo, GPU against CPU: $($c[1]) of $($c[0]) pixels over 8 levels (largest $($c[2]))"
            } else { Add-Check '3c.planets.sunphoto' $false "planet_picture ran ($($p.Err.Trim()))" }
        } else { Add-Check '3f.sun_photo_shown' $false "the app logged the Sun ($($suns.Count) PLSUN lines)" }
    }
}

# ---- 3d: the full benchmark, one run, at the owner's window (no PL_WINDOW: the app's own size) ----
if ($Parts -contains 'bench') {
    $hd = Join-Path $res 'home_bench'
    if (Test-Path $hd) { Remove-Item -Recurse -Force $hd }
    New-Item -ItemType Directory -Force $hd | Out-Null
    # the owner's window: maximised (2560 x 1369 inside the taskbar), as his own settings keep it
    Set-Content -Path (Join-Path $hd 'settings.txt') -Value $(if ($WindowAt) { "window=2560,1369,$WindowAt,1" } else { 'window=2560,1369,-,-,1' }) -Encoding ASCII
    $r = Invoke-Tool $app @() 'bench' @{ PL_BENCH = 'start'; PL_SETTINGS = (Join-Path $hd 'settings.txt'); PL_SHOT_DIR = $hd; PL_EXIT_AFTER_SECONDS = '1140' } 1200 $hd 'physics-lab-benchmark-*.txt'
    $file = Get-ChildItem $hd -Filter 'physics-lab-benchmark-*.txt' -ErrorAction SilentlyContinue | Select-Object -First 1
    Add-Check '3d.bench' ($null -ne $file) "the benchmark ran and saved its result ($(if ($file) { $file.Name } else { 'no file' }); $([math]::Round($r.Seconds)) s)"
    if ($file) {
        Copy-Item $file.FullName (Join-Path $res $file.Name) -Force
        foreach ($ln in (Get-Content $file.FullName | Where-Object { $_ -match 'Hz|refresh|ui\.resize_blackhole|rest\.orbit|rest\.blackhole_glare|blackhole\.close_spin|blackhole\.thick_spin|orbit\.true_light|orbit\.sun_edge|orbit\.oort_region|orbit\.porkchop|response\.porkchop_grid|orbit\.sun_surface|response\.sun_photo' } | Select-Object -First 40)) { Add-Info '3d.bench' $ln.Trim() }
    }
}

# ---- 3e: the open items that need the owner's hardware, each in a short benchmark run of its own ----
function Invoke-BenchOnly([string]$name, [string]$only, [hashtable]$more) {
    $hd = Join-Path $res "home_$name"
    if (Test-Path $hd) { Remove-Item -Recurse -Force $hd }
    New-Item -ItemType Directory -Force $hd | Out-Null
    Set-Content -Path (Join-Path $hd 'settings.txt') -Value $(if ($WindowAt) { "window=2560,1369,$WindowAt,1" } else { 'window=2560,1369,-,-,1' }) -Encoding ASCII
    $e = @{ PL_BENCH = 'start'; PL_BENCH_ONLY = $only; PL_SETTINGS = (Join-Path $hd 'settings.txt'); PL_SHOT_DIR = $hd; PL_EXIT_AFTER_SECONDS = '600' }
    if ($more) { foreach ($k in $more.Keys) { $e[$k] = $more[$k] } }
    $r = Invoke-Tool $app @() $name $e 660 $hd 'physics-lab-benchmark-*.txt'
    $file = Get-ChildItem $hd -Filter 'physics-lab-benchmark-*.txt' -ErrorAction SilentlyContinue | Select-Object -First 1
    $r | Add-Member File $file
    $r | Add-Member Log (Join-Path $hd 'gpu-paths-log.txt')
    return $r
}
function Get-SceneLines($file, [string]$id) {   # the result file's lines about one scene: its line and the indented lines under it
    if (-not $file) { return @() }
    $out = @(); $on = $false
    foreach ($ln in (Get-Content $file.FullName)) {
        if ($ln -match [regex]::Escape($id)) { $on = $true; $out += $ln.Trim(); continue }
        if ($on -and $ln -match '^\s{2,}\S') { $out += $ln.Trim() } else { $on = $false }
    }
    return $out
}
if ($Parts -contains 'items') {
    # rest.orbit: the Orbit Lab at rest drawn anew every frame (0.75.0: 766 drawings in 3 s). The log names the key's parts that changed.
    $r = Invoke-BenchOnly 'item_rest_orbit' 'rest.orbit' @{}
    foreach ($ln in (Get-SceneLines $r.File 'rest.orbit')) { Add-Info '3e.rest.orbit' $ln }
    # 0.85.3: the check is the rest line itself (quiet, no redraws in the counted 3 s). The log's "drawn anew" line can come from the lab's start, while
    # its card lines are still being decided (before the rest is counted), so it is shown as information only.
    $quiet = @(Get-SceneLines $r.File 'rest.orbit' | Where-Object { $_ -match '^rest\.orbit\s+quiet\s' -and $_ -match '\sms\s+no\s' })
    $anew = @(); if (Test-Path $r.Log) { $anew = @(Get-Content $r.Log | Where-Object { $_ -match 'drawn anew for' }) }
    if ($anew.Count) { Add-Info '3e.rest.orbit' ("the log: " + ($anew[0] -replace '^\[vulkan\] \[\w+\] \[\s*[\d.]+ ms\]\s*', '')) }
    Add-Check '3e.rest.orbit' ($r.File -and $quiet.Count -gt 0) "the Orbit Lab at rest keeps its picture (the rest line: $(if ($quiet.Count) { 'quiet, no redraws' } else { 'busy or redrawn' }))"
    # rest.blackhole_glare: busy at rest (45 % CPU in an older run); the same with the moving gas off (PL_BH_REST_NOGAS)
    foreach ($g in @(@('item_glare_gas', @{}), @('item_glare_nogas', @{ PL_BH_REST_NOGAS = '1' }))) {
        $r = Invoke-BenchOnly $g[0] 'rest.blackhole_glare' $g[1]
        foreach ($ln in (Get-SceneLines $r.File 'rest.blackhole_glare')) { Add-Info "3e.$($g[0])" $ln }
        if (-not $r.File) { Add-Check "3e.$($g[0])" $false 'the rest line ran and was saved' }
    }
    # ui.resize_blackhole from a maximised window (the scene restores it first and maximises it after)
    $r = Invoke-BenchOnly 'item_resize_max' 'ui.resize_blackhole' @{}
    foreach ($ln in (Get-SceneLines $r.File 'ui.resize_blackhole')) { Add-Info '3e.resize.maximised' $ln }
    $saved = Get-Content (Join-Path $res 'home_item_resize_max\settings.txt') -ErrorAction SilentlyContinue | Where-Object { $_ -like 'window=*' }
    Add-Check '3e.resize.maximised' ($r.File -and $saved -match ',1$') "the scene ran from a maximised window and left it maximised (settings: $saved)"
    if ($NoKeys) { Add-Info '3e.fullscreen' 'skipped (-NoKeys)' } else {
    # full screen: F11 pressed twice through Windows (the app's own key) over the Black Hole Lab, at the screen's own size
    $hd = Join-Path $res 'home_item_fullscreen'
    if (Test-Path $hd) { Remove-Item -Recurse -Force $hd }
    New-Item -ItemType Directory -Force $hd | Out-Null
    $keys = Start-Job -ScriptBlock {
        $sh = New-Object -ComObject WScript.Shell
        foreach ($t in 6, 7) { Start-Sleep $t; [void]$sh.AppActivate('Physics Lab'); Start-Sleep -Milliseconds 300; $sh.SendKeys('{F11}') }
    }
    $r = Invoke-Tool $app @() 'item_fullscreen' @{ PL_BLACKHOLE = '1'; PL_SETTINGS = (Join-Path $hd 'settings.txt'); PL_SHOT_DIR = $hd; PL_EXIT_AFTER_SECONDS = '22' } 120 $hd
    Receive-Job $keys -Wait -ErrorAction SilentlyContinue | Out-Null; Remove-Job $keys -Force
    $sw = @(); $log = Join-Path $hd 'gpu-paths-log.txt'
    if (Test-Path $log) { $sw = @(Get-Content $log | Where-Object { $_ -match 'swapchain' } | ForEach-Object { $_ -replace '^\[vulkan\] \[\s*[\d.]+ ms\]\s*', '' }) }
    $scr = Get-CimInstance Win32_VideoController | Where-Object { $_.CurrentHorizontalResolution } | Select-Object -First 1
    $full = "$($scr.CurrentHorizontalResolution) x $($scr.CurrentVerticalResolution)"
    Add-Check '3e.fullscreen' ($r.Code -eq 0 -and ($sw -match [regex]::Escape($full))) "F11 twice over the Black Hole Lab: exit $($r.Code), a swapchain at the screen's $full ($(@($sw | Select-Object -Unique) -join ' | '))"
    }
}

# ---- 3b: gpu-paths-log.txt, every [vulkan] line; a path that fell back to the CPU is a finding ----
if ($Parts -contains 'logs') {
    $logs = @(Get-ChildItem $res -Recurse -Filter 'gpu-paths-log.txt' -ErrorAction SilentlyContinue)
    $own = Join-Path $env:APPDATA 'PhysicsLab\PhysicsLab\gpu-paths-log.txt'
    if (Test-Path $own) { $logs += Get-Item $own }   # (read only: the owner's own log from his last use of the app)
    # every [vulkan] line of every log goes to results\logs-vulkan.txt; the summary lists the lines that say a path is off, died or draws anew
    $all = Join-Path $res 'logs-vulkan.txt'
    Set-Content -Path $all -Value '' -Encoding UTF8
    $seen = @{}
    foreach ($l in $logs) {
        $name = $(if ($l.FullName -eq $own) { "the owner's own log ($own)" } else { $l.Directory.Name })
        $lines = @(Get-Content $l.FullName | Where-Object { $_ -match '^\[vulkan\]' })
        $swap = @($lines | Where-Object { $_ -match 'swapchain made again' }).Count
        Add-Content -Path $all -Value "==== $name ($($lines.Count) [vulkan] lines, $swap of them 'swapchain made again')" -Encoding UTF8
        Add-Content -Path $all -Value ($lines | Where-Object { $_ -notmatch 'swapchain made again' }) -Encoding UTF8
        Add-Info 'logs.vulkan' "$name : $($lines.Count) [vulkan] lines ($swap swapchain rebuilds)"
        foreach ($ln in $lines) {
            $t = ($ln -replace '\[\s*[\d.]+ ms\]\s*', '')   # (the time stamp off: the same line from several runs is listed once)
            $bad = $t -match '(?i)switched off|is off|turned off|died|fell back|not made|could not|crash|drawn anew' -and $t -notmatch '(?i)passed|succeeded'
            if ($bad -and -not $seen.ContainsKey($t)) { $seen[$t] = $name; Add-Check 'logs.vulkan' $false "$t   [$name]" }
        }
    }
    if (-not $logs.Count) { Add-Info 'logs.vulkan' 'no gpu-paths-log.txt found' }
}

# ---- 3g: the Vulkan validation layers, if installed ----
if ($Parts -contains 'validation') {
    $have = $false
    foreach ($key in @('HKLM:\SOFTWARE\Khronos\Vulkan\ExplicitLayers', 'HKCU:\SOFTWARE\Khronos\Vulkan\ExplicitLayers')) {
        $it = Get-Item $key -ErrorAction SilentlyContinue
        if ($it) { foreach ($n in $it.Property) { if ($n -match 'VkLayer_khronos_validation') { $have = $true } } }
    }
    if (-not $have) { Add-Info '3g.validation' 'the Khronos validation layers are not installed: skipped' }
    else {
        foreach ($lab in $labs.Keys) {
            $r = Invoke-App "validation_$lab" (Merge $labs[$lab] @{ PL_VK_VALIDATION = '1' }) 30 300
            $msgs = @($r.Err -split "`r?`n" | Where-Object { $_.StartsWith('PLVKVALIDATION') })
            Add-Check "3g.validation.$lab" ($msgs.Count -eq 0 -and $r.Out -match 'PLVK validation errors=0 warnings=0') "no validation message ($($msgs.Count); first: $($msgs | Select-Object -First 1))"
        }
    }
}

Write-Line "DONE  $script:nPass passed, $script:nFail failed"
