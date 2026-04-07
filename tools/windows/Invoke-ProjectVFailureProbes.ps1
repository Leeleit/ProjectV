param(
    [string]$ExePath = "$PSScriptRoot\..\..\build\windows-clang-debug\bin\ProjectV.exe",
    [int]$ExitTimeoutMs = 15000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedExePath = [System.IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $resolvedExePath)) {
    throw "ProjectV executable not found: $resolvedExePath"
}

$tempRoot = Join-Path $env:TEMP ("ProjectVFailureProbes-" + [guid]::NewGuid().ToString("N"))
$null = New-Item -ItemType Directory -Path $tempRoot -Force
$emptyShaderDir = Join-Path $tempRoot "empty-shaders"
$null = New-Item -ItemType Directory -Path $emptyShaderDir -Force

function Invoke-ProjectVProbe {
    param(
        [string]$Name,
        [hashtable]$EnvironmentOverrides,
        [string]$ExpectedStderrPattern,
        [string]$ExpectedDescription
    )

    $stdoutPath = Join-Path $tempRoot "$Name.stdout.txt"
    $stderrPath = Join-Path $tempRoot "$Name.stderr.txt"
    Remove-Item -LiteralPath $stdoutPath,$stderrPath -ErrorAction SilentlyContinue

    $savedEnvironment = @{}
    foreach ($entry in $EnvironmentOverrides.GetEnumerator()) {
        $savedEnvironment[$entry.Key] = [Environment]::GetEnvironmentVariable($entry.Key, "Process")
        [Environment]::SetEnvironmentVariable($entry.Key, [string]$entry.Value, "Process")
    }

    try {
        Write-Host "Running probe: $Name"
        $process = Start-Process -FilePath $resolvedExePath -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        if (-not $process.WaitForExit($ExitTimeoutMs)) {
            Stop-Process -Id $process.Id -Force
            throw "Probe '$Name' timed out after $ExitTimeoutMs ms."
        }

        if ($process.ExitCode -eq 0) {
            throw "Probe '$Name' unexpectedly exited with code 0."
        }

        $stderrText = if (Test-Path -LiteralPath $stderrPath) {
            Get-Content -LiteralPath $stderrPath -Raw
        } else {
            ""
        }

        if ($stderrText -notmatch $ExpectedStderrPattern) {
            throw "Probe '$Name' did not emit expected stderr pattern for $ExpectedDescription.`nStderr:`n$stderrText"
        }

        Write-Host "Probe '$Name' passed."
    }
    finally {
        foreach ($entry in $EnvironmentOverrides.GetEnumerator()) {
            [Environment]::SetEnvironmentVariable($entry.Key, $savedEnvironment[$entry.Key], "Process")
        }
    }
}

try {
    Invoke-ProjectVProbe `
        -Name "missing-shader" `
        -EnvironmentOverrides @{ PROJECTV_SHADER_BASE_DIR = $emptyShaderDir } `
        -ExpectedStderrPattern "shader blob not found" `
        -ExpectedDescription "missing shader failure"

    Invoke-ProjectVProbe `
        -Name "incomplete-init" `
        -EnvironmentOverrides @{ PROJECTV_FAIL_INIT_STAGE = "before_voxel_meshing_pipeline" } `
        -ExpectedStderrPattern "intentional failure probe requested before voxel meshing pipeline creation" `
        -ExpectedDescription "intentional incomplete init failure"

    Write-Host "ProjectV failure probes passed."
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
