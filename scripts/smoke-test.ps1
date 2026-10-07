# Smoke test for OpenGothic on Windows: start the game, load a world, check that it neither crashes nor hangs.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -GothicPath "D:\Games\Gothic II"
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Build              # build Gothic2Notr first
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Mode idle -Seconds 45 -ExtraArgs "-game:Mod.ini"
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Marvin "set time 13 0;zstartrain 0.5" -ScreenshotAt "12,20"
#
# Modes:
#   benchmark (default)  Runs with `-benchmark ci`: the world's TIMEDEMO camera path is played, FPS is logged and the
#                        game exits by itself. Pass = clean exit, "Exiting benchmark" in log, no crash.log.
#                        Vanilla newworld.zen has a TIMEDEMO camera; other worlds/mods may not, use idle there.
#   idle                 Runs for -Seconds, then kills the game. Pass = still running at that point, no crash.log.
#
# The game runs in its own working directory (build/smoke by default), so log.txt, crash.log and Gothic.ini
# of the run end up there and do not mix with manual play sessions. A summary is written to summary.json.
#
# -Marvin runs console commands once the world is loaded (game flag `-marvin`, ';'-separated); a command that
# fails counts as a test failure. -ScreenshotAt captures the game window at the given seconds after start
# (shot_<N>s.png in the run directory); the window has to be visible on the desktop.
# Exit code: 0 = pass, 1 = fail.

param(
  [string]$GothicPath = $env:OPENGOTHIC_GOTHIC_PATH,
  [ValidateSet("benchmark", "idle")]
  [string]$Mode = "benchmark",
  [int]$Seconds = 30,
  [int]$TimeoutSeconds = 180,
  [string]$World = "",
  [string[]]$ExtraArgs = @(),
  [string]$BuildDir = "build",
  [string]$RunDir = "",
  [string]$Marvin = "",
  [string]$ScreenshotAt = "",
  [switch]$Build
)

$ErrorActionPreference = "Stop"

function Save-GameScreenshots($Proc, $Stopwatch, [string]$At, [string]$Dir) {
  Add-Type -AssemblyName System.Drawing
  if(-not ("SmokeWin" -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SmokeWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
    }
  [SmokeWin]::SetProcessDPIAware() | Out-Null
  foreach($t in ($At.Split(',') | ForEach-Object { [int]$_ })) {
    while($Stopwatch.Elapsed.TotalSeconds -lt $t -and -not $Proc.HasExited) { Start-Sleep -Milliseconds 200 }
    if($Proc.HasExited) { break }
    $Proc.Refresh()
    $h = $Proc.MainWindowHandle
    [SmokeWin]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
    $r = New-Object SmokeWin+RECT
    [SmokeWin]::GetWindowRect($h, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $file = Join-Path $Dir ("shot_{0:D2}s.png" -f $t)
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    $file
    }
  }
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if(-not $GothicPath) {
  # set after this shell started? read it from the user environment in the registry
  $GothicPath = [Environment]::GetEnvironmentVariable("OPENGOTHIC_GOTHIC_PATH", "User")
  }
if(-not $GothicPath) {
  throw "No Gothic path. Pass -GothicPath or set OPENGOTHIC_GOTHIC_PATH."
  }
if(-not (Test-Path (Join-Path $GothicPath "Data"))) {
  throw "Not a Gothic installation (no Data folder): $GothicPath"
  }

if($Build) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build-windows.ps1") -BuildDir $BuildDir -Target Gothic2Notr
  if($LASTEXITCODE -ne 0) {
    Write-Host "SMOKE FAIL: build failed"
    exit 1
    }
  }

$exe = Join-Path $root "$BuildDir\opengothic\Gothic2Notr.exe"
if(-not (Test-Path $exe)) {
  throw "Game executable not found: $exe (build first, or pass -Build)"
  }

if(-not $RunDir) {
  $RunDir = Join-Path $root "$BuildDir\smoke"
  }
if(Test-Path $RunDir) {
  Remove-Item -Recurse -Force $RunDir
  }
New-Item -ItemType Directory -Force $RunDir | Out-Null

$gameArgs = @("-g", "`"$GothicPath`"", "-nomenu", "-window")
if($Mode -eq "benchmark") {
  $gameArgs += @("-benchmark", "ci")
  }
if($World) {
  $gameArgs += @("-w", $World)
  }
if($Marvin) {
  $gameArgs += @("-marvin", "`"$Marvin`"")
  }
$gameArgs += $ExtraArgs

Write-Host "Running: $exe $($gameArgs -join ' ')"
Write-Host "Working directory: $RunDir"

$sw = [Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $exe -ArgumentList $gameArgs -WorkingDirectory $RunDir -PassThru

$shots = @()
if($ScreenshotAt) {
  $shots = @(Save-GameScreenshots -Proc $proc -Stopwatch $sw -At $ScreenshotAt -Dir $RunDir)
  }

$failures = @()
if($Mode -eq "benchmark") {
  $exited = $proc.WaitForExit($TimeoutSeconds * 1000)
  if(-not $exited) {
    $failures += "did not exit within $TimeoutSeconds s (hang, or world has no TIMEDEMO camera)"
    Stop-Process -Id $proc.Id -Force
    $proc.WaitForExit()
    }
  elseif($proc.ExitCode -ne 0) {
    $failures += "exit code $($proc.ExitCode)"
    }
  }
else {
  $left   = [math]::Max(0, $Seconds*1000 - [int]$sw.Elapsed.TotalMilliseconds)
  $exited = $proc.WaitForExit($left)
  if($exited) {
    $failures += "exited after $([int]$sw.Elapsed.TotalSeconds) s with code $($proc.ExitCode) before the $Seconds s window ended"
    }
  else {
    Stop-Process -Id $proc.Id -Force
    $proc.WaitForExit()
    }
  }
$duration = [math]::Round($sw.Elapsed.TotalSeconds, 1)

$logFile   = Join-Path $RunDir "log.txt"
$crashFile = Join-Path $RunDir "crash.log"
$log = @()
if(Test-Path $logFile) {
  # plain strings, otherwise ConvertTo-Json serializes Get-Content's PS* note properties
  $log = @(Get-Content $logFile | ForEach-Object { [string]$_ })
  }
else {
  $failures += "no log.txt written"
  }
if(Test-Path $crashFile) {
  $failures += "crash.log written"
  }
if($log | Select-String -SimpleMatch "invalid gothic path") {
  $failures += "game did not accept the Gothic path"
  }
foreach($m in ($log | Select-String -Pattern '^marvin: (.*) failed$')) {
  $failures += "console command failed: $($m.Matches[0].Groups[1].Value)"
  }

$fps = $null
$low1 = $null
$bench = $log | Select-String -Pattern 'Benchmark: low 1% = ([\d.]+) fps = ([\d.]+)' | Select-Object -Last 1
if($bench) {
  $low1 = [double]$bench.Matches[0].Groups[1].Value
  $fps  = [double]$bench.Matches[0].Groups[2].Value
  }
if($Mode -eq "benchmark" -and -not ($log | Select-String -SimpleMatch "Exiting benchmark")) {
  $failures += "benchmark did not finish ('Exiting benchmark' missing from log)"
  }

# Unique non-zenkit warnings, useful for diffing two runs (before/after a change).
$warnings = $log |
  Where-Object { $_ -notmatch '^\[zenkit\]' -and $_ -match 'not implemented|unable|invalid|error|fail|exception' } |
  Sort-Object -Unique

$passed = $failures.Count -eq 0
$summary = [ordered]@{
  passed          = $passed
  mode            = $Mode
  durationSeconds = $duration
  fps             = $fps
  low1PercentFps  = $low1
  failures        = $failures
  warningCount    = @($warnings).Count
  warnings        = @($warnings)
  screenshots     = @($shots)
  logFile         = $logFile
  }
$summary | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 (Join-Path $RunDir "summary.json")

Write-Host ""
Write-Host "Duration: $duration s"
if($fps) {
  Write-Host "FPS: $fps (low 1%: $low1)"
  }
Write-Host "Unique warnings in log: $(@($warnings).Count) (see summary.json)"
$shots | ForEach-Object { Write-Host "Screenshot: $_" }
if(Test-Path $crashFile) {
  Write-Host "--- crash.log (tail)"
  Get-Content $crashFile -Tail 30 | ForEach-Object { Write-Host $_ }
  }

if($passed) {
  Write-Host "SMOKE PASS"
  exit 0
  }
Write-Host "SMOKE FAIL:"
$failures | ForEach-Object { Write-Host "  - $_" }
Write-Host "--- log.txt (tail)"
$log | Select-Object -Last 20 | ForEach-Object { Write-Host $_ }
exit 1
