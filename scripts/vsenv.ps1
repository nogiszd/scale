<#
.SYNOPSIS
    Imports the MSVC x64 developer environment into the current PowerShell session.

.DESCRIPTION
    Dot-source this script:  . .\scripts\vsenv.ps1
    It locates Visual Studio (or Build Tools) with the x64 C++ toolset through
    vswhere, runs vcvars64.bat and copies the resulting environment into this
    session only. It then puts the CMake and Ninja bundled with Visual Studio
    first on PATH (session only) so builds do not pick up unrelated copies
    (for example Strawberry Perl's). Nothing machine-wide is modified.
#>
[CmdletBinding()]
param(
    [switch] $KeepPathOrder
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at '$vswhere'. Install Visual Studio Build Tools with the 'Desktop development with C++' workload."
}

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) {
    throw 'No Visual Studio installation with the MSVC x64 toolset (Microsoft.VisualStudio.Component.VC.Tools.x86.x64) was found.'
}

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at '$vcvars'."
}

$envDump = cmd.exe /d /s /c "`"$vcvars`" >nul 2>&1 && set" 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "vcvars64.bat failed with exit code $LASTEXITCODE."
}
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
    }
}

if (-not $KeepPathOrder) {
    $bundled = @(
        (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'),
        (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja')
    ) | Where-Object { Test-Path $_ }
    if ($bundled) {
        $env:PATH = (($bundled + ($env:PATH -split ';')) | Select-Object -Unique) -join ';'
    }
}

Write-Host "MSVC environment loaded from $vsPath (target: $env:VSCMD_ARG_TGT_ARCH, toolset: $env:VCToolsVersion, SDK: $env:WindowsSDKVersion)"
