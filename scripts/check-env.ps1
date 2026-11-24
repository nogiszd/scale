<#
.SYNOPSIS
    Reports which Scale toolchain components are available. Installs nothing.

.DESCRIPTION
    Run after dot-sourcing scripts/vsenv.ps1:
        . .\scripts\vsenv.ps1
        .\scripts\check-env.ps1
    Exit code 0 if every tool required for the CPU track is present, 1 otherwise.
    Graphics and shader track tools are reported but do not affect the exit code.
#>
[CmdletBinding()]
param()

$missingRequired = 0

function Get-ToolLine([string] $name, [scriptblock] $versionProbe) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cmd) { return $null }
    $version = try { (& $versionProbe $cmd.Source) -join ' ' } catch { '<version probe failed>' }
    return [pscustomobject]@{ Path = $cmd.Source; Version = $version.Trim() }
}

function Report([string] $track, [string] $name, [scriptblock] $probe, [string] $hint, [bool] $required) {
    $info = Get-ToolLine $name $probe
    if ($info) {
        Write-Host ("[ok]      {0,-10} {1,-14} {2}  ({3})" -f $track, $name, $info.Version, $info.Path)
    } else {
        $tag = if ($required) { '[MISSING]' } else { '[absent] ' }
        Write-Host ("{0} {1,-10} {2,-14} {3}" -f $tag, $track, $name, $hint)
        if ($required) { $script:missingRequired++ }
    }
}

$firstLine = { param($p) & $p --version 2>&1 | Select-Object -First 1 }

Write-Host '== CPU track (required) =='
Report 'cpu' 'git'     $firstLine 'Install Git for Windows: winget install Git.Git' $true
Report 'cpu' 'cmake'   $firstLine 'Bundled with Visual Studio C++ workload, or: winget install Kitware.CMake' $true
Report 'cpu' 'ninja'   $firstLine 'Bundled with Visual Studio C++ workload, or: winget install Ninja-build.Ninja' $true
Report 'cpu' 'cl'      { param($p) & $p 2>&1 | Select-Object -First 1 } 'Dot-source scripts/vsenv.ps1 (needs VS Build Tools, Desktop development with C++)' $true
Report 'cpu' 'link'    { param($p) 'present' } 'Dot-source scripts/vsenv.ps1' $true
Report 'cpu' 'dumpbin' { param($p) 'present' } 'Dot-source scripts/vsenv.ps1' $true
if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    Write-Host "[MISSING] cpu        target arch is '$env:VSCMD_ARG_TGT_ARCH', expected x64 (use vcvars64)"
    $missingRequired++
}
if ($env:WindowsSDKVersion) { Write-Host "[ok]      cpu        Windows SDK    $env:WindowsSDKVersion" }

Write-Host ''
Write-Host '== Graphics / shader tracks (needed from M6/M7) =='
Report 'graphics' 'vulkaninfo' { param($p) & $p --summary 2>&1 | Select-String 'deviceName|driverName|apiVersion' | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() } } 'Installed with the GPU driver or the Vulkan SDK' $false
Report 'graphics' 'spirv-val'  $firstLine 'Install the LunarG Vulkan SDK: https://vulkan.lunarg.com/sdk/home#windows (or winget install KhronosGroup.VulkanSDK)' $false
Report 'graphics' 'glslangValidator' $firstLine 'Part of the LunarG Vulkan SDK' $false
Report 'shader'   'llvm-mc'      $firstLine 'LLVM with AMDGPU target: winget install LLVM.LLVM, or https://github.com/llvm/llvm-project/releases' $false
Report 'shader'   'llvm-objdump' $firstLine 'Same LLVM distribution as llvm-mc' $false

$mc = Get-Command llvm-mc -ErrorAction SilentlyContinue
if ($mc) {
    $targets = & $mc.Source --version 2>&1 | Out-String
    if ($targets -match 'amdgcn') {
        Write-Host '[ok]      shader     AMDGPU target registered in llvm-mc'
    } else {
        Write-Host '[absent]  shader     llvm-mc lacks the AMDGPU (amdgcn) target; use an LLVM build with AMDGPU enabled'
    }
}
if ($env:VULKAN_SDK) { Write-Host "[ok]      graphics   VULKAN_SDK=$env:VULKAN_SDK" } else { Write-Host '[absent]  graphics   VULKAN_SDK is not set' }

Write-Host ''
if ($missingRequired -gt 0) {
    Write-Host "$missingRequired required tool(s) missing."
    exit 1
}
Write-Host 'All CPU track tools present.'
exit 0
