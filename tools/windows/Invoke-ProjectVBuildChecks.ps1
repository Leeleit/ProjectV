param(
    [string]$ConfigurePreset = "windows-clang-debug-ci",
    [string]$BuildPreset = "windows-clang-debug-ci-build",
    [string]$TestPreset = "windows-clang-debug-ci-tests",
    [switch]$RunSmoke,
    [string]$SmokeBuildPreset = "windows-clang-debug-smoke"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Label,
        [scriptblock]$Command
    )

    Write-Host "==> $Label"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

Invoke-Step -Label "Configure ($ConfigurePreset)" -Command {
    cmake --preset $ConfigurePreset
}

Invoke-Step -Label "Build ($BuildPreset)" -Command {
    cmake --build --preset $BuildPreset
}

Invoke-Step -Label "Tests ($TestPreset)" -Command {
    ctest --preset $TestPreset
}

if ($RunSmoke) {
    Invoke-Step -Label "Smoke ($SmokeBuildPreset)" -Command {
        cmake --build --preset $SmokeBuildPreset
    }
}
