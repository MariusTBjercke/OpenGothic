# Builds OpenGothic on Windows with MSVC + Ninja.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1 [-Config RelWithDebInfo] [-Target Gothic2Notr] [-Clean]
# Requires: Visual Studio with the C++ workload, CMake, Ninja, Vulkan SDK (for glslangValidator).

param(
  [string]$Config = "RelWithDebInfo",
  [string[]]$Target = @("Gothic2Notr", "Spacer"),
  [string]$BuildDir = "build",
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Pick up tools installed after this shell was started (winget updates PATH in the registry only).
$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
if(-not $env:VULKAN_SDK) {
  $env:VULKAN_SDK = [Environment]::GetEnvironmentVariable("VULKAN_SDK", "Machine")
  }
if($env:VULKAN_SDK) {
  $env:Path = "$env:VULKAN_SDK\Bin;$env:Path"
  }

foreach($tool in "cmake", "ninja", "glslangValidator") {
  if(-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
    throw "$tool not found on PATH"
    }
  }

if(-not (Test-Path "lib/Tempest/Engine/CMakeLists.txt")) {
  git submodule update --init --recursive
  }

# Import the MSVC x64 developer environment.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs) {
  throw "Visual Studio with the C++ workload not found"
  }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>nul && set" | ForEach-Object {
  if($_ -match "^([^=]+)=(.*)$") {
    Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
  }

if($Clean -and (Test-Path $BuildDir)) {
  Remove-Item -Recurse -Force $BuildDir
  }

cmake -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDir --target $Target
exit $LASTEXITCODE
