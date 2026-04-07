param(
    [string]$ExePath = "$PSScriptRoot\..\..\build\windows-clang-debug\bin\ProjectV.exe",
    [int]$StartupTimeoutMs = 15000,
    [int]$ShutdownTimeoutMs = 10000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedExePath = [System.IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $resolvedExePath)) {
    throw "ProjectV executable not found: $resolvedExePath"
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
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
    }
}
