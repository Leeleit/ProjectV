param(
    [string]$ExePath = "$PSScriptRoot\..\..\build\windows-clang-debug\bin\ProjectV.exe",
    [int]$StartupTimeoutMs = 15000,
    [int]$ShutdownTimeoutMs = 10000,
    # **LookDev capture contract (`2026-06-15`).**
    # When `-CaptureDir` is supplied, set the same
    # `PROJECTV_SCREENSHOT_DIR` / `PROJECTV_START_CAMERA_*` /
    # `PROJECTV_LOOKDEV_CAPTURE_*` env vars that the Linux
    # `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` does, then
    # verify that the expected number of `.bmp` + `.txt` files
    # landed in `$CaptureDir` after the process exits. Mirrors
    # the `windows-clang-debug/lookdev-captures/2026-...-*/`
    # artifacts that previously had to be regenerated manually
    # (per `agent/memory.md §1`).
    [string]$CaptureDir = "",
    [string]$Views = "FINAL SHDW CSM CTSH AOCC LOCL",
    [string]$CameraPosition = "",
    [string]$CameraLook = "",
    [int]$WarmupFrames = 30,
    [int]$IntervalFrames = 2,
    [switch]$QuitAfterCapture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedExePath = [System.IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $resolvedExePath)) {
    throw "ProjectV executable not found: $resolvedExePath"
}

# **LookDev capture env-var setup (`2026-06-15`).**
# Same vars as the Linux bash script. Resolve to absolute
# paths so the operator can pass relative CaptureDir from
# anywhere; the engine's `src/render/ScreenshotCapture.cpp`
# uses SDL_GetBasePath for the file path so an absolute
# path here is the safe default.
$CaptureMode = -not [string]::IsNullOrWhiteSpace($CaptureDir)
if ($CaptureMode) {
    $CaptureDir = [System.IO.Path]::GetFullPath($CaptureDir)
    if (-not (Test-Path -LiteralPath $CaptureDir)) {
        New-Item -ItemType Directory -Path $CaptureDir -Force | Out-Null
    }
    $env:PROJECTV_SCREENSHOT_DIR = $CaptureDir
    if (-not [string]::IsNullOrWhiteSpace($CameraPosition)) {
        $env:PROJECTV_START_CAMERA_POSITION = $CameraPosition
    }
    if (-not [string]::IsNullOrWhiteSpace($CameraLook)) {
        $env:PROJECTV_START_CAMERA_LOOK = $CameraLook
    }
    $env:PROJECTV_LOOKDEV_CAPTURE_VIEWS = $Views
    $env:PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES = "$WarmupFrames"
    $env:PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES = "$IntervalFrames"
    if ($QuitAfterCapture) {
        $env:PROJECTV_LOOKDEV_CAPTURE_QUIT = "1"
    }
    Write-Host "LookDev capture: dir=$CaptureDir views=$Views"
}

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class ProjectVSmokeWin32 {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(
        IntPtr hWnd,
        IntPtr hWndInsertAfter,
        int X,
        int Y,
        int cx,
        int cy,
        uint uFlags);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);
}
"@

$SW_MAXIMIZE = 3
$SW_MINIMIZE = 6
$SW_RESTORE = 9
$SWP_NOMOVE = 0x0002
$SWP_NOZORDER = 0x0004
$SWP_NOACTIVATE = 0x0010

function Wait-MainWindowHandle {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutMs
    )

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    do {
        Start-Sleep -Milliseconds 100
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "ProjectV exited before the main window became available. Exit code: $($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne 0) {
            return $Process.MainWindowHandle
        }
    } while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs)

    throw "Timed out waiting for ProjectV main window handle."
}

function Resize-Window {
    param(
        [IntPtr]$Handle,
        [int]$Width,
        [int]$Height
    )

    $ok = [ProjectVSmokeWin32]::SetWindowPos(
        $Handle,
        [IntPtr]::Zero,
        0,
        0,
        $Width,
        $Height,
        $SWP_NOMOVE -bor $SWP_NOZORDER -bor $SWP_NOACTIVATE)
    if (-not $ok) {
        throw "SetWindowPos failed for ${Width}x${Height}."
    }
    Start-Sleep -Milliseconds 600
}

$process = $null
try {
    Write-Host "Starting ProjectV: $resolvedExePath"
    $process = Start-Process -FilePath $resolvedExePath -PassThru
    $mainWindowHandle = Wait-MainWindowHandle -Process $process -TimeoutMs $StartupTimeoutMs

    Write-Host "Resize -> 1280x720"
    Resize-Window -Handle $mainWindowHandle -Width 1280 -Height 720

    Write-Host "Resize -> 960x540"
    Resize-Window -Handle $mainWindowHandle -Width 960 -Height 540

    Write-Host "Minimize"
    [void][ProjectVSmokeWin32]::ShowWindowAsync($mainWindowHandle, $SW_MINIMIZE)
    Start-Sleep -Milliseconds 800

    Write-Host "Restore"
    [void][ProjectVSmokeWin32]::ShowWindowAsync($mainWindowHandle, $SW_RESTORE)
    Start-Sleep -Milliseconds 800

    Write-Host "Maximize"
    [void][ProjectVSmokeWin32]::ShowWindowAsync($mainWindowHandle, $SW_MAXIMIZE)
    Start-Sleep -Milliseconds 800

    Write-Host "Restore"
    [void][ProjectVSmokeWin32]::ShowWindowAsync($mainWindowHandle, $SW_RESTORE)
    Start-Sleep -Milliseconds 800

    Write-Host "Requesting graceful shutdown"
    if (-not $process.CloseMainWindow()) {
        throw "CloseMainWindow failed."
    }

    if (-not $process.WaitForExit($ShutdownTimeoutMs)) {
        throw "ProjectV did not exit within $ShutdownTimeoutMs ms after CloseMainWindow."
    }

    if ($process.ExitCode -ne 0) {
        throw "ProjectV exited with non-zero code: $($process.ExitCode)"
    }

    Write-Host "ProjectV runtime smoke passed."

    # **LookDev capture file verification (`2026-06-15`).**
    if ($CaptureMode) {
        $expectedCount = ($Views -split '[ ,|]+' | Where-Object { $_ -ne '' }).Count
        $bmpFiles = Get-ChildItem -LiteralPath $CaptureDir -Filter '*.bmp' -File -ErrorAction SilentlyContinue
        $txtFiles = Get-ChildItem -LiteralPath $CaptureDir -Filter '*.txt' -File -ErrorAction SilentlyContinue
        Write-Host "LookDev capture verification: dir=$CaptureDir"
        Write-Host "  expected views : $expectedCount"
        Write-Host "  found .bmp     : $($bmpFiles.Count)"
        Write-Host "  found .txt     : $($txtFiles.Count)"

        if ($bmpFiles.Count -lt $expectedCount) {
            throw "LookDev capture: expected at least $expectedCount .bmp files, found $($bmpFiles.Count)"
        }
        if ($txtFiles.Count -lt $expectedCount) {
            throw "LookDev capture: expected at least $expectedCount .txt sidecar files, found $($txtFiles.Count)"
        }
        Write-Host "LookDev capture verification passed."
    }
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
    }
}
