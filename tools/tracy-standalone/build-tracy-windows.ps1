#!/usr/bin/env pwsh
# **Tracy Profiler UI standalone build — Windows (`2026-06-15`).**
#
# Configures and builds the Tracy UI as a separate top-level CMake project
# at external/tracy/profiler. Run this AFTER building ProjectV with
# PROJECTV_ENABLE_TRACY=ON (via the windows-clang-debug-tracy-profiler
# preset); the resulting ProjectV.exe will then connect to the Tracy UI
# at 127.0.0.1:8086 when you launch it.
#
# Usage:
#   .\tools\tracy-standalone\build-tracy-windows.ps1
#   .\tools\tracy-standalone\build-tracy-windows.ps1 -BuildDir build\windows-clang-tracy
#   .\tools\tracy-standalone\build-tracy-windows.ps1 -BuildDir build\windows-clang-tracy -ConfigureOnly
#   .\tools\tracy-standalone\build-tracy-windows.ps1 -BuildDir build\windows-clang-tracy -Compiler clang-cl.exe

[CmdletBinding()]
param(
    [string]$BuildDir = "build\windows-clang-tracy",
    [string]$Compiler = "clang-cl.exe",
    [switch]$ConfigureOnly,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($Help) {
    Get-Help $PSCommandPath -Detailed
    exit 0
}

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = (Resolve-Path "$ScriptDir\..\..").Path
$TracyProfilerSrc = Join-Path $RepoRoot "external\tracy\profiler"
$BuildDirFull = Join-Path $RepoRoot $BuildDir
$CpmSourceCache = Join-Path $RepoRoot "build\cpm-source-cache"

if (-not (Test-Path $TracyProfilerSrc)) {
    Write-Error "Tracy profiler source not found at: $TracyProfilerSrc"
}

if (-not (Test-Path $Compiler)) {
    # Try to find clang-cl via LLVM toolchain
    $LLVMCl = Get-Command $Compiler -ErrorAction SilentlyContinue
    if ($null -eq $LLVMCl) {
        Write-Error "Compiler '$Compiler' not found in PATH. Install LLVM 22+ and ensure clang-cl.exe is reachable, or pass -Compiler <full-path>."
    }
}

Write-Host "[tracy-standalone] Tracy profiler source : $TracyProfilerSrc"
Write-Host "[tracy-standalone] Build dir             : $BuildDirFull"
Write-Host "[tracy-standalone] CPM source cache      : $CpmSourceCache"
Write-Host "[tracy-standalone] Compiler              : $Compiler"

# Configure. Tracy profiler's CMakeLists.txt has its own project()
# call (project(tracy-profiler)), so we can't add_subdirectory it
# from a parent CMakeLists. The standalone build is the canonical
# way to compile the Tracy UI without colliding with ProjectV's
# own nlohmann_json FetchContent target (CMP0002).
$ConfigureArgs = @(
    "-S"        ; $TracyProfilerSrc
    "-B"        ; $BuildDirFull
    "-G"        ; "Ninja"
    "-A"        ; "x64"
    "-DCMAKE_C_COMPILER=$Compiler"
    "-DCMAKE_CXX_COMPILER=$Compiler"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DPROJECTV_BUILD_TRACY_PROFILER=ON"
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    "-DCMAKE_DISABLE_FIND_PACKAGE_rocprofiler-sdk=TRUE"
    "-DCPM_SOURCE_CACHE=$CpmSourceCache"
    "-DNO_ISA_EXTENSIONS=ON"
    "-DBASE64_WERROR=OFF"
    "-DBUILD_LIBCURL_DOCS=OFF"
    "-DBUILD_MISC_DOCS=OFF"
    "-DENABLE_CURL_MANUAL=OFF"
    "-DCMAKE_MESSAGE_LOG_LEVEL=NOTICE"
    "-DCMAKE_WARN_DEPRECATED=OFF"
    "-DCMAKE_SUPPRESS_DEVELOPER_WARNINGS=TRUE"
    "-DCMAKE_POLICY_DEFAULT_CMP0069=NEW"
    "-DCMAKE_C_FLAGS=/clang:-Wno-unused-command-line-argument /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS"
    "-DCMAKE_CXX_FLAGS=/clang:-Wno-unused-command-line-argument /EHsc /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS"
)

Write-Host "[tracy-standalone] Configuring..."
& cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "cmake configure failed (exit $LASTEXITCODE)"
}

if ($ConfigureOnly) {
    Write-Host "[tracy-standalone] Configure-only mode; skipping build."
    exit 0
}

Write-Host "[tracy-standalone] Building (tracy-profiler + tracy-capture)..."
& cmake --build $BuildDirFull --target tracy-profiler tracy-capture
if ($LASTEXITCODE -ne 0) {
    Write-Error "cmake build failed (exit $LASTEXITCODE)"
}

Write-Host ""
Write-Host "[tracy-standalone] Done. Run the Tracy UI to connect to ProjectV:"
Write-Host "  $($BuildDirFull)\bin\tracy-profiler.exe"
Write-Host ""
Write-Host "[tracy-standalone] ProjectV must have been built with PROJECTV_ENABLE_TRACY=ON"
Write-Host "[tracy-standalone] (use cmake --preset windows-clang-debug-tracy-profiler)."
