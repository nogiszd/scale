<#
.SYNOPSIS
    Builds llvm-mc and llvm-objdump with the AMDGPU target from a pinned LLVM
    source release. Needed by the shader track (M7) for decoder cross-checks.

.DESCRIPTION
    The official LLVM Windows release archives and the winget package do not
    register the AMDGPU target (and winget lacks llvm-mc). This script builds
    just the two tools with LLVM_TARGETS_TO_BUILD=AMDGPU using the MSVC
    toolchain located by scripts/vsenv.ps1. No administrator rights needed.

    Steps: download the source tarball from GitHub, verify its pinned SHA-256,
    extract the required directories, configure with CMake/Ninja, build,
    copy the two executables to <InstallRoot>\bin, and check that
    `llvm-mc -triple=amdgcn -mcpu=gfx1030` assembles an instruction.

    Typical run time: 10-30 minutes, about 3 GB of temporary disk space.

.EXAMPLE
    .\scripts\build-llvm-amdgpu.ps1
    .\scripts\build-llvm-amdgpu.ps1 -AddToUserPath -Clean
#>
[CmdletBinding()]
param(
    # Pinned release. Update the version and hash together, from the
    # `digest` of llvm-project-<ver>.src.tar.xz on the GitHub release page.
    [string] $Version = '23.1.6',
    [string] $Sha256 = 'd3b86ab29cfb4445a6652e5c2642ccc75e04e38084dde3701d323320f35a31da',

    # Where the tools are installed (bin\ is created below it).
    [string] $InstallRoot = (Join-Path $env:LOCALAPPDATA "Programs\LLVM-AMDGPU\$Version"),

    # Source, build tree and download cache. Keep this path short: LLVM's
    # source tree is deep and Windows paths are limited to 260 characters
    # for some tools.
    [string] $WorkDir = (Join-Path $env:LOCALAPPDATA "Programs\LLVM-AMDGPU\work-$Version"),

    # Parallel build jobs (default: Ninja's choice).
    [int] $Jobs = 0,

    # Add <InstallRoot>\bin to the *user* PATH (persistent, not machine-wide).
    [switch] $AddToUserPath,

    # Delete the work directory (source, build tree, download) after success.
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$ProgressPreference = 'SilentlyContinue'

function Invoke-Checked([string] $what, [scriptblock] $block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

$srcName = "llvm-project-$Version.src"
$archive = Join-Path $WorkDir "$srcName.tar.xz"
$src = Join-Path $WorkDir $srcName
$build = Join-Path $WorkDir 'build'
$bin = Join-Path $InstallRoot 'bin'

New-Item -ItemType Directory -Force $WorkDir | Out-Null

# ---- Download and verify ----------------------------------------------------
if (-not (Test-Path $archive)) {
    $url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$Version/$srcName.tar.xz"
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $archive -UserAgent 'scale-build-llvm'
}
$actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $Sha256.ToLowerInvariant()) {
    Remove-Item $archive
    throw "SHA-256 mismatch for $archive (expected $Sha256, got $actual). The download was deleted."
}
Write-Host "Source verified: sha256 $actual"

# ---- Extract only what the build needs ---------------------------------------
# llvm/: the tools. cmake/, third-party/: shared CMake modules and headers.
# libc/: LLVM 23's configure requires its shared utilities (observed: a
#        fatal "LLVM libc is not found" error without it).
# Tests and the mlgo-utils symlinks (which tar cannot create on Windows) are
# skipped.
if (-not (Test-Path (Join-Path $src 'llvm\CMakeLists.txt')) -or
    -not (Test-Path (Join-Path $src 'libc\CMakeLists.txt'))) {
    Write-Host "Extracting to $WorkDir"
    Invoke-Checked 'tar' {
        tar -xf $archive -C $WorkDir `
            --exclude "$srcName/llvm/test" `
            --exclude "$srcName/llvm/unittests" `
            --exclude "$srcName/llvm/utils/mlgo-utils" `
            --exclude "$srcName/libc/test" `
            "$srcName/llvm" "$srcName/cmake" "$srcName/third-party" "$srcName/libc"
    }
}

# ---- Configure and build ------------------------------------------------------
. (Join-Path $PSScriptRoot 'vsenv.ps1')

Invoke-Checked 'CMake configure' {
    cmake -S (Join-Path $src 'llvm') -B $build -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl `
        -DLLVM_TARGETS_TO_BUILD=AMDGPU `
        "-DLLVM_ENABLE_PROJECTS=" `
        -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF `
        -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_DOCS=OFF `
        -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF `
        -DLLVM_ENABLE_DIA_SDK=OFF
}

$buildArgs = @('--build', $build, '--target', 'llvm-mc', 'llvm-objdump')
if ($Jobs -gt 0) { $buildArgs += @('--parallel', $Jobs) }
Invoke-Checked 'build' { cmake @buildArgs }

# ---- Install and verify --------------------------------------------------------
New-Item -ItemType Directory -Force $bin | Out-Null
Copy-Item (Join-Path $build 'bin\llvm-mc.exe'), (Join-Path $build 'bin\llvm-objdump.exe') $bin -Force

$mc = Join-Path $bin 'llvm-mc.exe'
$versionText = & $mc --version | Out-String
if ($versionText -notmatch 'amdgcn') {
    throw "Built llvm-mc does not register the amdgcn target:`n$versionText"
}
$encoding = "v_add_nc_u32 v0, v1, v2`ns_endpgm" | & $mc -triple=amdgcn -mcpu=gfx1030 -show-encoding | Out-String
if ($LASTEXITCODE -ne 0 -or $encoding -notmatch 'encoding:') {
    throw "llvm-mc could not assemble a gfx1030 instruction:`n$encoding"
}
Write-Host $encoding.Trim()

if ($AddToUserPath) {
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $entries = @($userPath -split ';' | Where-Object { $_ })
    if ($entries -notcontains $bin) {
        [Environment]::SetEnvironmentVariable('Path', (($entries + $bin) -join ';'), 'User')
        Write-Host "Added $bin to the user PATH (open a new shell or dot-source scripts/vsenv.ps1)."
    }
}

if ($Clean) {
    Remove-Item -Recurse -Force $WorkDir
    Write-Host "Removed $WorkDir"
}

Write-Host "LLVM $Version AMDGPU tools installed to $bin"
