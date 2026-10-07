# Smoke test for OpenGothic on Windows: start the game, load a world, check that it neither crashes nor hangs.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -GothicPath "D:\Games\Gothic II"
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Build              # build Gothic2Notr first
#   powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Mode idle -Seconds 45 -ExtraArgs "-game:Mod.ini"
#
# Modes:
#   benchmark (default)  Runs with `-benchmark ci`: the world's TIMEDEMO camera path is played, FPS is logged and the
#                        game exits by itself. Pass = clean exit, "Exiting benchmark" in log, no crash.log.
#                        Vanilla newworld.zen has a TIMEDEMO camera; other worlds/mods may not, use idle there.
#   idle                 Runs for -Seconds, then kills the game. Pass = still running at that point, no crash.log.
#
# The game runs in its own working directory (build/smoke by default), so log.txt, crash.log and Gothic.ini
# of the run end up there and do not mix with manual play sessions. A summary is written to summary.json.
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
  [switch]$Build
)

$ErrorActionPreference = "Stop"
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
$gameArgs += $ExtraArgs

Write-Host "Running: $exe $($gameArgs -join ' ')"
Write-Host "Working directory: $RunDir"

$sw = [Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $exe -ArgumentList $gameArgs -WorkingDirectory $RunDir -PassThru

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
  $exited = $proc.WaitForExit($Seconds * 1000)
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
  logFile         = $logFile
  }
$summary | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 (Join-Path $RunDir "summary.json")

Write-Host ""
Write-Host "Duration: $duration s"
if($fps) {
  Write-Host "FPS: $fps (low 1%: $low1)"
  }
Write-Host "Unique warnings in log: $(@($warnings).Count) (see summary.json)"
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
