<#
.SYNOPSIS
    Configures and builds Voxcraft inside a Visual Studio developer environment.
.EXAMPLE
    .\scripts\build.ps1                 # debug build
    .\scripts\build.ps1 -Config release # optimized build
    .\scripts\build.ps1 -Run            # build then launch the game
    .\scripts\build.ps1 -Clean          # delete the build dir first
#>
param(
    [ValidateSet('debug', 'release')]
    [string]$Config = 'debug',
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if ($Clean -and (Test-Path "$root\build\$Config")) {
    Remove-Item -Recurse -Force "$root\build\$Config"
}

# Locate the VS C++ toolchain (cl, ninja) via vswhere.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) { throw 'No Visual Studio installation with C++ tools found.' }

$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>&1 && cd /d `"$root`" && cmake --preset $Config && cmake --build --preset $Config"
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

if ($Run) {
    Start-Process "$root\build\$Config\bin\Voxcraft.exe" -WorkingDirectory $root
}
