# Copies the current build into a stable play folder outside the repository, after building and smoke testing it.
# Saves (save_slot_N.sav) and OpenGothic's Gothic.ini live in that folder and are never touched by this script.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/deploy-play.ps1                 # build, smoke test, deploy
#   powershell -ExecutionPolicy Bypass -File scripts/deploy-play.ps1 -SkipSmoke      # build and deploy
#   powershell -ExecutionPolicy Bypass -File scripts/deploy-play.ps1 -SkipBuild -SkipSmoke
#   powershell -ExecutionPolicy Bypass -File scripts/deploy-play.ps1 -DesktopShortcut
#
# Play folder: -Dest, else $env:OPENGOTHIC_PLAY_DIR, else "OpenGothic-play" next to the Gothic installation.
# Gothic path: -GothicPath, else $env:OPENGOTHIC_GOTHIC_PATH (also read from the user registry env).

param(
  [string]$GothicPath = $env:OPENGOTHIC_GOTHIC_PATH,
  [string]$Dest = $env:OPENGOTHIC_PLAY_DIR,
  [string]$BuildDir = "build",
  [switch]$SkipBuild,
  [switch]$SkipSmoke,
  [switch]$DesktopShortcut
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if(-not $GothicPath) {
  $GothicPath = [Environment]::GetEnvironmentVariable("OPENGOTHIC_GOTHIC_PATH", "User")
  }
if(-not $GothicPath -or -not (Test-Path (Join-Path $GothicPath "Data"))) {
  throw "No valid Gothic path. Pass -GothicPath or set OPENGOTHIC_GOTHIC_PATH."
  }
if(-not $Dest) {
  $Dest = [Environment]::GetEnvironmentVariable("OPENGOTHIC_PLAY_DIR", "User")
  }
if(-not $Dest) {
  $Dest = Join-Path (Split-Path -Parent $GothicPath) "OpenGothic-play"
  }

if(-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build-windows.ps1") -BuildDir $BuildDir -Target Gothic2Notr
  if($LASTEXITCODE -ne 0) { throw "build failed, play folder not updated" }
  }
if(-not $SkipSmoke) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "smoke-test.ps1") -GothicPath $GothicPath -BuildDir $BuildDir
  if($LASTEXITCODE -ne 0) { throw "smoke test failed, play folder not updated" }
  }

$src = Join-Path $root "$BuildDir\opengothic"
if(-not (Test-Path (Join-Path $src "Gothic2Notr.exe"))) {
  throw "no build found in $src"
  }

$running = Get-Process Gothic2Notr -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($Dest, [StringComparison]::OrdinalIgnoreCase) }
if($running) {
  throw "OpenGothic is running from $Dest, close it first"
  }

New-Item -ItemType Directory -Force $Dest | Out-Null
Get-ChildItem $src -File | Where-Object { $_.Extension -in ".exe", ".dll", ".pdb" -and $_.Name -notlike "Spacer*" } |
  ForEach-Object { Copy-Item $_.FullName -Destination $Dest -Force }

$commit = (git rev-parse --short HEAD).Trim()
$branch = (git rev-parse --abbrev-ref HEAD).Trim()
$dirty  = if(git status --porcelain --untracked-files=no) { " (with uncommitted changes)" } else { "" }
@(
  "branch: $branch",
  "commit: $commit$dirty",
  "built:  $((Get-Item (Join-Path $src 'Gothic2Notr.exe')).LastWriteTime.ToString('yyyy-MM-dd HH:mm'))",
  "deployed: $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
) | Set-Content -Encoding ascii (Join-Path $Dest "BUILD.txt")

# launchers; %~dp0 keeps saves and Gothic.ini in this folder whatever the shortcut's start directory is
@"
@echo off
cd /d "%~dp0"
start "" Gothic2Notr.exe -g "$GothicPath" %*
"@ | Set-Content -Encoding ascii (Join-Path $Dest "Play.bat")
@"
@echo off
rem same as Play.bat with marvin mode on: F2 opens the console (e.g. zstartrain 0.5)
rem no -window here: that is a debug mode where the mouse is never captured for the camera
cd /d "%~dp0"
start "" Gothic2Notr.exe -g "$GothicPath" -devmode %*
"@ | Set-Content -Encoding ascii (Join-Path $Dest "Play (devmode).bat")

# Script Patch (scripts only, its Union plugins do not apply), if its ini is installed in the Gothic folder.
# Saves of patched scripts must not mix with vanilla ones, so it runs in its own working directory.
Get-ChildItem (Join-Path $GothicPath "system") -Filter "g2a_nr_scriptpatch_*.ini" -ErrorAction SilentlyContinue |
  ForEach-Object {
@"
@echo off
rem Gothic II Script Patch scripts; saves and Gothic.ini live in ScriptPatch\
if not exist "%~dp0ScriptPatch" mkdir "%~dp0ScriptPatch"
cd /d "%~dp0ScriptPatch"
start "" "%~dp0Gothic2Notr.exe" -g "$GothicPath" -game:$($_.Name) %*
"@ | Set-Content -Encoding ascii (Join-Path $Dest "Play (Script Patch).bat")
  }

if($DesktopShortcut) {
  $desktop = [Environment]::GetFolderPath("Desktop")
  $sh  = New-Object -ComObject WScript.Shell
  $lnk = $sh.CreateShortcut((Join-Path $desktop "OpenGothic (fork).lnk"))
  $lnk.TargetPath       = Join-Path $Dest "Gothic2Notr.exe"
  $lnk.Arguments        = "-g `"$GothicPath`""
  $lnk.WorkingDirectory = $Dest
  $lnk.IconLocation     = (Join-Path $Dest "Gothic2Notr.exe") + ",0"
  $lnk.Save()
  Write-Host "Desktop shortcut: $(Join-Path $desktop 'OpenGothic (fork).lnk')"
  }

Write-Host "Deployed $branch@$commit$dirty to $Dest"
Write-Host "Start: $(Join-Path $Dest 'Play.bat')"
