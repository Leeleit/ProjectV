# Invoke-ProjectVProfile.ps1 — unattended Windows CLI profiling for ProjectV.
#
# Produces under OutDir:
#   summary.json   — machine-readable facts for the agent
#   SUMMARY.md     — short human digest
#   app.log        — ProjectV stdout/stderr (when captured by the harness)
#   tool-specific artifacts (nsys-rep, ngfx-capture, ncu-rep, rdc, CSV stats)
#
# Tools (Nsight trinity + RenderDoc):
#   Systems        — Nsight Systems timeline (default; best first bottleneck pass)
#   GraphicsCapture — ngfx-capture + ngfx-replay --perf-report-dir
#   GpuTrace       — Nsight Graphics GPU Trace Profiler (+ auto-export metrics)
#   Compute        — Nsight Compute (kernel deep-dive; slow / high overhead)
#   RenderDoc      — inject + wait; needs F12 during run (no in-app TriggerCapture yet)
#
# Env-var contract (ProjectV binary ignores argv; use env only):
#   PROJECTV_BENCHMARK_FRAMES / WARMUP_FRAMES / QUIT
#   PROJECTV_SCENE_PRESET, PROJECTV_ENABLE_VALIDATION
#
# Usage:
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Smoke
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Frames 120 -Warmup 30
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GraphicsCapture -CaptureFrame 45
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GpuTrace -StartAfterFrames 60 -LimitFrames 2
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Compute -LaunchCount 3
#   .\tools\windows\Invoke-ProjectVProfile.ps1 -Tool RenderDoc -Frames 90   # press F12 mid-run
#
# Exit codes: 0 ok, 1 usage, 2 missing tool/exe, 3 capture/profile failed, 5 ProjectV non-zero

[CmdletBinding()]
param(
    [ValidateSet('Systems', 'GraphicsCapture', 'GpuTrace', 'Compute', 'RenderDoc')]
    [string]$Tool = 'Systems',

    [string]$ExePath = '',
    [string]$BuildDir = '',
    [string]$OutDir = '',
    [string]$Label = '',

    [int]$Frames = 120,
    [int]$Warmup = 30,
    [string]$ScenePreset = 'VoxelLab',
    [ValidateSet('ON', 'OFF')]
    [string]$Validation = 'OFF',

    # GraphicsCapture
    [int]$CaptureFrame = 45,
    [int]$ReplayLoops = 30,

    # GpuTrace
    [int]$StartAfterFrames = 60,
    [int]$LimitFrames = 2,
    [int]$MaxDurationMs = 2000,
    [string]$GpuArchitecture = '',

    # Compute (ncu)
    [int]$LaunchCount = 5,
    [string]$NcuSet = 'full',

    [switch]$Smoke,
    [switch]$SkipStats,
    [switch]$WhatIf
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDir '..\..'))
. (Join-Path $scriptDir 'Resolve-ProjectVProfilerTools.ps1')
$tools = Resolve-ProjectVProfilerTools

if ($Smoke) {
    $Frames = 45
    $Warmup = 10
    $CaptureFrame = 30
    $StartAfterFrames = 25
    $LimitFrames = 1
    $MaxDurationMs = 1000
    $ReplayLoops = 5
    $LaunchCount = 1
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $projectRoot 'build\windows-clang-debug'
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $BuildDir 'bin\ProjectV.exe'
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)

if (-not (Test-Path -LiteralPath $ExePath)) {
    Write-Error "ProjectV executable not found: $ExePath`nBuild first: cmake --build --preset windows-clang-debug-build --target ProjectV"
    exit 2
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($Label)) {
    $Label = "$Tool-$ScenePreset-$stamp"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $BuildDir "profiler-captures\$Label"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($GpuArchitecture)) {
    $GpuArchitecture = $tools.GpuArchitecture
}

$workDir = Split-Path -Parent $ExePath
$appLog = Join-Path $OutDir 'app.log'
$summaryPath = Join-Path $OutDir 'summary.json'
$summaryMdPath = Join-Path $OutDir 'SUMMARY.md'

function Set-ProjectVBenchmarkEnv {
    param([hashtable]$Extra = @{})
    $env:PROJECTV_BENCHMARK_FRAMES = "$Frames"
    $env:PROJECTV_BENCHMARK_WARMUP_FRAMES = "$Warmup"
    $env:PROJECTV_BENCHMARK_QUIT = '1'
    $env:PROJECTV_SCENE_PRESET = $ScenePreset
    $env:PROJECTV_ENABLE_VALIDATION = $Validation
    # Nsight Graphics + Khronos validation = known AV in driver/layer stack; also mute OBS/RTSS hooks.
    if ($Tool -in @('GraphicsCapture', 'GpuTrace', 'Compute')) {
        $env:PROJECTV_ENABLE_VALIDATION = 'OFF'
        $env:DISABLE_VK_LAYER_KHRONOS_validation = '1'
        $env:DISABLE_VK_LAYER_OBS_HOOK = '1'
        $env:DISABLE_VK_LAYER_RTSS = '1'
        $Validation = 'OFF'
    }
    foreach ($key in $Extra.Keys) {
        Set-Item -Path "Env:$key" -Value "$($Extra[$key])"
    }
}

function Clear-ProjectVBenchmarkEnv {
    @(
        'PROJECTV_BENCHMARK_FRAMES'
        'PROJECTV_BENCHMARK_WARMUP_FRAMES'
        'PROJECTV_BENCHMARK_QUIT'
        'PROJECTV_SCENE_PRESET'
        'PROJECTV_ENABLE_VALIDATION'
        'DISABLE_VK_LAYER_KHRONOS_validation'
        'DISABLE_VK_LAYER_OBS_HOOK'
        'DISABLE_VK_LAYER_RTSS'
    ) | ForEach-Object {
        Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue
    }
}

function Parse-BenchmarkLine {
    param([string]$LogPath)
    $result = [ordered]@{
        mean_ms  = $null
        min_ms   = $null
        max_ms   = $null
        mean_fps = $null
        min_fps  = $null
        max_fps  = $null
        frames   = $null
        tris     = $null
        nonair   = $null
        raw      = $null
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        return [pscustomobject]$result
    }
    $line = Select-String -Path $LogPath -Pattern '\[ProjectV\]\[BenchmarkAutomation\] done ' |
        Select-Object -Last 1
    if (-not $line) {
        return [pscustomobject]$result
    }
    $text = $line.Line
    $result.raw = $text
    if ($text -match 'frames=(\d+)') { $result.frames = [int]$Matches[1] }
    if ($text -match 'mean_ms=([0-9.]+)') { $result.mean_ms = [double]$Matches[1] }
    if ($text -match 'min_ms=([0-9.]+)') { $result.min_ms = [double]$Matches[1] }
    if ($text -match 'max_ms=([0-9.]+)') { $result.max_ms = [double]$Matches[1] }
    if ($text -match 'mean_fps=([0-9.]+)') { $result.mean_fps = [double]$Matches[1] }
    if ($text -match 'min_fps=([0-9.]+)') { $result.min_fps = [double]$Matches[1] }
    if ($text -match 'max_fps=([0-9.]+)') { $result.max_fps = [double]$Matches[1] }
    if ($text -match 'tris=(\d+)') { $result.tris = [int]$Matches[1] }
    if ($text -match 'nonair=(\d+)') { $result.nonair = [int]$Matches[1] }
    return [pscustomobject]$result
}

function Import-TopCsvRows {
    param(
        [string]$CsvPath,
        [int]$Top = 15
    )
    if (-not (Test-Path -LiteralPath $CsvPath)) {
        return @()
    }
    try {
        $rows = Import-Csv -LiteralPath $CsvPath
        if (-not $rows) { return @() }
        $timeCol = @('Time (%)', 'Total Time (ns)', 'Total Time (s)', 'Duration') |
            Where-Object { $rows[0].PSObject.Properties.Name -contains $_ } |
            Select-Object -First 1
        if ($timeCol) {
            $rows = $rows | Sort-Object {
                $v = $_.$timeCol
                if ($null -eq $v -or $v -eq '') { 0.0 } else { [double]$v }
            } -Descending
        }
        return @($rows | Select-Object -First $Top)
    } catch {
        return @()
    }
}

function Write-ProfileSummary {
    param(
        [hashtable]$Summary
    )
    $Summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

    $bench = $Summary.benchmark
    $lines = @(
        "# ProjectV profile: $($Summary.label)"
        ''
        "- **Tool:** $($Summary.tool)"
        "- **Scene:** $($Summary.scene)"
        "- **Validation:** $($Summary.validation)"
        "- **Exe:** $($Summary.exe)"
        "- **OutDir:** $($Summary.outDir)"
        "- **Status:** $($Summary.status)"
    )
    if ($bench -and $bench.mean_ms) {
        $lines += "- **Benchmark mean:** $($bench.mean_ms) ms ($($bench.mean_fps) fps) over $($bench.frames) frames"
        $lines += "- **Benchmark range:** min=$($bench.min_ms) ms / max=$($bench.max_ms) ms"
    }
    if ($Summary.notes -and $Summary.notes.Count -gt 0) {
        $lines += ''
        $lines += '## Notes'
        foreach ($n in $Summary.notes) {
            $lines += "- $n"
        }
    }
    if ($Summary.topMarkers -and $Summary.topMarkers.Count -gt 0) {
        $lines += ''
        $lines += '## Top markers / kernels (first rows)'
        foreach ($row in $Summary.topMarkers) {
            $lines += "- ``$($row | ConvertTo-Json -Compress)``"
        }
    }
    $lines += ''
    $lines += '## Artifacts'
    foreach ($key in $Summary.artifacts.Keys) {
        $val = $Summary.artifacts[$key]
        $lines += "- **${key}:** $val"
    }
    $lines -join "`n" | Set-Content -LiteralPath $summaryMdPath -Encoding utf8
}

function Require-Tool {
    param([string]$Path, [string]$Name)
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        Write-Error "$Name CLI not found. Install Nsight/RenderDoc or set PROJECTV_* override env."
        exit 2
    }
}

Write-Host "profile: tool=$Tool label=$Label"
Write-Host "profile: exe=$ExePath"
Write-Host "profile: out=$OutDir"
Write-Host "profile: frames=$Frames warmup=$Warmup scene=$ScenePreset validation=$Validation"

$summary = [ordered]@{
    schemaVersion = 1
    label         = $Label
    tool          = $Tool
    timestampUtc  = (Get-Date).ToUniversalTime().ToString('o')
    exe           = $ExePath
    buildDir      = $BuildDir
    outDir        = $OutDir
    scene         = $ScenePreset
    validation    = $Validation
    frames        = $Frames
    warmup        = $Warmup
    status        = 'running'
    exitCode      = $null
    benchmark     = $null
    artifacts     = [ordered]@{}
    topMarkers    = @()
    notes         = [System.Collections.Generic.List[string]]::new()
    toolVersions  = [ordered]@{}
}

if ($WhatIf) {
    $summary.status = 'whatif'
    $summary.notes.Add('WhatIf only - no capture executed.')
    Write-ProfileSummary -Summary $summary
    Write-Host "profile: WHATIF - wrote $summaryPath"
    exit 0
}

try {
    switch ($Tool) {
        'Systems' {
            Require-Tool -Path $tools.Nsys -Name 'Nsight Systems (nsys)'
            $summary.toolVersions.nsys = (& $tools.Nsys --version 2>&1 | Out-String).Trim()
            $repBase = Join-Path $OutDir 'nsys-report'
            $repFile = "$repBase.nsys-rep"
            $statsDir = Join-Path $OutDir 'nsys-stats'
            New-Item -ItemType Directory -Path $statsDir -Force | Out-Null

            Set-ProjectVBenchmarkEnv
            $errLog = Join-Path $OutDir 'nsys.err.log'
            $nsysArgs = @(
                'profile'
                '--force-overwrite=true'
                '--stats=true'
                "--output=$repBase"
                '--trace=vulkan,vulkan-annotations,nvtx'
                '--vulkan-gpu-workload=individual'
                '--sample=none'
                '--cpuctxsw=none'
                '--kill=true'
                $ExePath
            )
            Write-Host "profile: nsys $($nsysArgs -join ' ')"
            $proc = Start-Process -FilePath $tools.Nsys -ArgumentList $nsysArgs `
                -WorkingDirectory $workDir -NoNewWindow -PassThru `
                -RedirectStandardOutput $appLog -RedirectStandardError $errLog
            $proc.WaitForExit()
            $proc.Refresh()
            $summary.exitCode = $proc.ExitCode

            $errText = if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog -Raw } else { '' }
            $needsAdmin = $errText -match 'registry writing permissions|administrative privileges'
            if ($needsAdmin -and -not (Test-Path -LiteralPath $repFile)) {
                $summary.notes.Add('Vulkan nsys injection needs one elevated register (HKLM). Falling back to --trace=nvtx (no GPU Vulkan workload). Run tools/windows/Register-ProjectVNsightVulkanLayer.ps1 as Admin for full Systems traces.')
                $nsysArgs = @(
                    'profile'
                    '--force-overwrite=true'
                    '--stats=true'
                    "--output=$repBase"
                    '--trace=nvtx'
                    '--sample=none'
                    '--cpuctxsw=none'
                    '--kill=true'
                    $ExePath
                )
                Write-Host "profile: nsys FALLBACK $($nsysArgs -join ' ')"
                $proc = Start-Process -FilePath $tools.Nsys -ArgumentList $nsysArgs `
                    -WorkingDirectory $workDir -NoNewWindow -PassThru `
                    -RedirectStandardOutput $appLog -RedirectStandardError $errLog
                $proc.WaitForExit()
                $proc.Refresh()
                $summary.exitCode = $proc.ExitCode
            }
            Clear-ProjectVBenchmarkEnv

            if (-not (Test-Path -LiteralPath $repFile)) {
                $summary.status = 'failed'
                $summary.notes.Add("nsys did not produce $repFile (exit=$($proc.ExitCode))")
                Write-ProfileSummary -Summary $summary
                exit 3
            }
            $summary.artifacts.nsysRep = $repFile
            $summary.artifacts.appLog = $appLog
            $summary.artifacts.nsysErrLog = $errLog

            if (-not $SkipStats) {
                $statsArgs = @(
                    'stats'
                    '--force-export=true'
                    '--format=csv'
                    "--output=$statsDir"
                    '--report=vulkan_marker_sum'
                    '--report=vulkan_gpu_marker_sum'
                    '--report=nvtx_sum'
                    $repFile
                )
                Write-Host "profile: nsys stats ..."
                & $tools.Nsys @statsArgs 2>&1 | Tee-Object -FilePath (Join-Path $OutDir 'nsys-stats.log') | Out-Null
                $summary.artifacts.statsDir = $statsDir
                $cpuCsv = Get-ChildItem -LiteralPath $statsDir -Filter '*vulkan_marker_sum*.csv' -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                $gpuCsv = Get-ChildItem -LiteralPath $statsDir -Filter '*vulkan_gpu_marker_sum*.csv' -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                $top = @()
                if ($gpuCsv) {
                    $top += Import-TopCsvRows -CsvPath $gpuCsv.FullName
                    $summary.artifacts.vulkanGpuMarkerCsv = $gpuCsv.FullName
                }
                if ($cpuCsv) {
                    $top += Import-TopCsvRows -CsvPath $cpuCsv.FullName
                    $summary.artifacts.vulkanMarkerCsv = $cpuCsv.FullName
                }
                $summary.topMarkers = $top
                $summary.notes.Add('Open .nsys-rep in Nsight Systems GUI for full timeline; CSV is top-marker digest.')
            }
        }

        'GraphicsCapture' {
            Require-Tool -Path $tools.NgfxCapture -Name 'ngfx-capture'
            Require-Tool -Path $tools.NgfxReplay -Name 'ngfx-replay'
            $captureName = 'frame-capture.ngfx-capture'
            $perfDir = Join-Path $OutDir 'ngfx-perf'
            New-Item -ItemType Directory -Path $perfDir -Force | Out-Null

            # Do NOT use PROJECTV_BENCHMARK_QUIT here: app must stay alive until ngfx
            # finishes the capture + --terminate-after-capture. Validation OFF is mandatory
            # (Khronos validation + interception = AV).
            $env:PROJECTV_SCENE_PRESET = $ScenePreset
            $env:PROJECTV_ENABLE_VALIDATION = 'OFF'
            $env:DISABLE_VK_LAYER_KHRONOS_validation = '1'
            $env:DISABLE_VK_LAYER_OBS_HOOK = '1'
            $env:DISABLE_VK_LAYER_RTSS = '1'
            $Validation = 'OFF'

            $capArgs = @(
                '--exe', $ExePath
                '--working-dir', $workDir
                '--output-dir', $OutDir
                '--output-file', $captureName
                '--capture-frame', "$CaptureFrame"
                '--terminate-after-capture'
                '--no-block-on-interfering-application'
                '--env', "PROJECTV_SCENE_PRESET=$ScenePreset"
                '--env', 'PROJECTV_ENABLE_VALIDATION=OFF'
                '--env', 'DISABLE_VK_LAYER_KHRONOS_validation=1'
                '--env', 'DISABLE_VK_LAYER_OBS_HOOK=1'
                '--env', 'DISABLE_VK_LAYER_RTSS=1'
            )
            Write-Host "profile: ngfx-capture frame=$CaptureFrame (no auto-quit; terminate-after-capture)"
            $capLog = Join-Path $OutDir 'ngfx-capture.log'
            $capErr = Join-Path $OutDir 'ngfx-capture.err.log'
            $proc = Start-Process -FilePath $tools.NgfxCapture -ArgumentList $capArgs `
                -WorkingDirectory $workDir -NoNewWindow -PassThru `
                -RedirectStandardOutput $capLog -RedirectStandardError $capErr
            $waitMs = if ($Smoke) { 120000 } else { 300000 }
            $finished = $proc.WaitForExit($waitMs)
            if (-not $finished) {
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
                Get-Process -Name ProjectV -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
                $summary.notes.Add("ngfx-capture timed out after ${waitMs}ms")
            }
            $proc.Refresh()
            Clear-ProjectVBenchmarkEnv
            $summary.exitCode = $proc.ExitCode
            $summary.artifacts.captureLog = $capLog
            $summary.artifacts.captureErrLog = $capErr

            $found = Get-ChildItem -LiteralPath $OutDir -Filter '*.ngfx-capture' -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if (-not $found) {
                $summary.status = 'failed'
                $summary.notes.Add("ngfx-capture produced no .ngfx-capture (exit=$($proc.ExitCode))")
                $summary.notes.Add('If NVIDIA crash dialog mentioned validation/interception: ensure PROJECTV_ENABLE_VALIDATION=OFF (now runtime). Close Steam overlay if captures stay empty.')
                Write-ProfileSummary -Summary $summary
                exit 3
            }
            $capturePath = $found.FullName
            $summary.artifacts.ngfxCapture = $capturePath

            $replayLog = Join-Path $OutDir 'ngfx-replay.log'
            $repArgs = @(
                '--perf-report-dir', $perfDir
                '--loop-count', "$ReplayLoops"
                $capturePath
            )
            Write-Host "profile: ngfx-replay loops=$ReplayLoops"
            $rproc = Start-Process -FilePath $tools.NgfxReplay -ArgumentList $repArgs `
                -WorkingDirectory $OutDir -NoNewWindow -PassThru `
                -RedirectStandardOutput $replayLog -RedirectStandardError (Join-Path $OutDir 'ngfx-replay.err.log')
            $null = $rproc.WaitForExit(300000)
            $rproc.Refresh()
            $summary.artifacts.perfReportDir = $perfDir
            $summary.artifacts.replayLog = $replayLog
            if ($rproc.ExitCode -ne 0) {
                $summary.notes.Add("ngfx-replay exit=$($rproc.ExitCode) - check replay log")
            }
            if (Test-Path -LiteralPath $replayLog) {
                $fpsLine = Select-String -Path $replayLog -Pattern 'replayAdjustedFps|Adjusted FPS|FPS' |
                    Select-Object -First 5
                foreach ($l in $fpsLine) { $summary.notes.Add($l.Line.Trim()) }
            }
            Get-ChildItem -LiteralPath $perfDir -Recurse -File -ErrorAction SilentlyContinue |
                Select-Object -First 20 | ForEach-Object {
                    $summary.notes.Add("perf artifact: $($_.FullName)")
                }
        }

        'GpuTrace' {
            Require-Tool -Path $tools.Ngfx -Name 'ngfx (GPU Trace)'
            $traceOut = Join-Path $OutDir 'gpu-trace'
            New-Item -ItemType Directory -Path $traceOut -Force | Out-Null
            $Frames = [Math]::Max($Frames, $StartAfterFrames + $LimitFrames + 30)
            Set-ProjectVBenchmarkEnv

            $envBlob = "PROJECTV_BENCHMARK_FRAMES=$Frames; PROJECTV_BENCHMARK_WARMUP_FRAMES=$Warmup; PROJECTV_BENCHMARK_QUIT=1; PROJECTV_SCENE_PRESET=$ScenePreset; PROJECTV_ENABLE_VALIDATION=OFF; DISABLE_VK_LAYER_KHRONOS_validation=1; DISABLE_VK_LAYER_OBS_HOOK=1; DISABLE_VK_LAYER_RTSS=1; PROJECTV_PRESENT_MODE=MAILBOX; PROJECTV_FULLSCREEN=1"
            Write-Host "profile: ngfx GPU Trace arch=$GpuArchitecture after=$StartAfterFrames limit=$LimitFrames"
            $gLog = Join-Path $OutDir 'ngfx-gputrace.log'
            $gErr = Join-Path $OutDir 'ngfx-gputrace.err.log'
            $ngfxHost = Split-Path -Parent $tools.Ngfx
            # Nsight Qt plugins live under Plugins\; Cursor Qt on PATH breaks qwindows init.
            # Launch via cmd with ngfx-first PATH (no Cursor) — still may fail: ngfx.exe --activity
            # crashes 0xC0000409 (Qt) on this host; prefer -Tool GraphicsCapture / Systems + in-app gpu_*.
            $userPath = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                [Environment]::GetEnvironmentVariable('Path', 'User')
            $safePath = "$ngfxHost;" + (($userPath -split ';' |
                    Where-Object { $_ -and $_ -notmatch '(?i)[\\/]cursor[\\/]|[\\/]vscode[\\/]' }) -join ';')
            $batPath = Join-Path $OutDir 'run-gputrace.bat'
            $bat = @"
@echo off
set PATH=$safePath
set QT_PLUGIN_PATH=$ngfxHost\Plugins
set QT_QPA_PLATFORM_PLUGIN_PATH=$ngfxHost\Plugins\platforms
set QT_QPA_PLATFORM=windows
cd /d "$ngfxHost"
"$tools.Ngfx" --activity "GPU Trace Profiler" --platform "Windows (x86_64)" --exe "$ExePath" --dir "$workDir" --output-dir "$traceOut" --env "$envBlob" --start-after-frames=$StartAfterFrames --limit-to-frames=$LimitFrames --max-duration-ms=$MaxDurationMs --auto-export --architecture=$GpuArchitecture --no-block-on-interfering-application > "$gLog" 2> "$gErr"
exit /b %ERRORLEVEL%
"@
            # Expand tools.Ngfx path into bat (avoid $tools in here-string confusion)
            $bat = $bat.Replace('"$tools.Ngfx"', "`"$($tools.Ngfx)`"")
            Set-Content -LiteralPath $batPath -Value $bat -Encoding ASCII
            Write-Host "profile: ngfx via $batPath (Cursor-stripped PATH)"
            $proc = Start-Process -FilePath 'cmd.exe' -ArgumentList "/c `"$batPath`"" `
                -WorkingDirectory $ngfxHost -NoNewWindow -PassThru
            $null = $proc.WaitForExit(600000)
            if (-not $proc.HasExited) {
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
                Get-Process -Name ngfx, ProjectV -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
                $summary.notes.Add('ngfx GPU Trace timed out after 600s')
            }
            $proc.Refresh()
            Clear-ProjectVBenchmarkEnv
            $summary.exitCode = $proc.ExitCode
            $summary.artifacts.gpuTraceDir = $traceOut
            $summary.artifacts.gpuTraceLog = $gLog
            $exports = @(Get-ChildItem -LiteralPath $traceOut -Recurse -Include *.csv,*.json,*.txt -ErrorAction SilentlyContinue)
            foreach ($f in $exports) {
                $summary.notes.Add("export: $($f.FullName)")
            }
            if ($summary.exitCode -eq -1073740791 -or $summary.exitCode -eq 0xC0000409) {
                $summary.status = 'failed'
                $summary.notes.Add('ngfx.exe --activity crashed (0xC0000409 / Qt platform plugin). Use -Tool GraphicsCapture or Systems; GPU Trace needs Nsight UI on this host.')
            } elseif ($summary.exitCode -ne 0) {
                $summary.status = 'failed'
                $summary.notes.Add("ngfx GPU Trace exit=$($summary.exitCode); see ngfx-gputrace.log / .err.log")
            }
            if (-not $exports) {
                $summary.notes.Add('No auto-export CSV/JSON found - open GPU Trace capture in Nsight Graphics UI.')
            }
        }

        'Compute' {
            Require-Tool -Path $tools.Ncu -Name 'Nsight Compute (ncu)'
            $ncuBase = Join-Path $OutDir 'ncu-report'
            Set-ProjectVBenchmarkEnv
            # ncu replays kernels; keep frame budget small unless operator overrides.
            if (-not $PSBoundParameters.ContainsKey('Frames') -and -not $Smoke) {
                $Frames = 40
                $env:PROJECTV_BENCHMARK_FRAMES = "$Frames"
            }
            $ncuArgs = @(
                '--set', $NcuSet
                '--target-processes', 'application-only'
                '--launch-count', "$LaunchCount"
                '--kill', '1'
                '-o', $ncuBase
                '--force-overwrite'
                '--export', "$ncuBase"
                '--csv'
                $ExePath
            )
            Write-Host "profile: ncu set=$NcuSet launches=$LaunchCount"
            $ncuLog = Join-Path $OutDir 'ncu.log'
            $proc = Start-Process -FilePath $tools.Ncu -ArgumentList $ncuArgs `
                -WorkingDirectory $workDir -NoNewWindow -PassThru `
                -RedirectStandardOutput $ncuLog -RedirectStandardError (Join-Path $OutDir 'ncu.err.log')
            $proc.WaitForExit()
            Clear-ProjectVBenchmarkEnv
            $summary.exitCode = $proc.ExitCode
            $summary.artifacts.ncuLog = $ncuLog
            $rep = Get-ChildItem -LiteralPath $OutDir -Filter 'ncu-report*' -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 5
            foreach ($r in $rep) {
                $summary.artifacts[$r.Name] = $r.FullName
            }
            $csv = Get-ChildItem -LiteralPath $OutDir -Filter '*.csv' -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($csv) {
                $summary.topMarkers = Import-TopCsvRows -CsvPath $csv.FullName
                $summary.artifacts.ncuCsv = $csv.FullName
            }
            $summary.notes.Add('NCU has high overhead; use after Systems/GpuTrace narrow the hot kernel.')
        }

        'RenderDoc' {
            Require-Tool -Path $tools.RenderDocCmd -Name 'renderdoccmd'
            $rdcTemplate = Join-Path $OutDir 'capture'
            Set-ProjectVBenchmarkEnv
            $summary.notes.Add('RenderDoc has no unattended frame trigger in-tree yet - press F12 (default) during the run to write .rdc.')
            $summary.notes.Add('Debug labels are ON in windows-clang-debug (PROJECTV_ENABLE_RENDERDOC_MARKERS).')
            $rdArgs = @(
                'capture'
                '--wait-for-exit'
                '--capture-file', $rdcTemplate
                '--working-dir', $workDir
                $ExePath
            )
            Write-Host "profile: renderdoccmd capture (press F12 to snap a frame)"
            $rdLog = Join-Path $OutDir 'renderdoc.log'
            $proc = Start-Process -FilePath $tools.RenderDocCmd -ArgumentList $rdArgs `
                -WorkingDirectory $workDir -NoNewWindow -PassThru `
                -RedirectStandardOutput $rdLog -RedirectStandardError (Join-Path $OutDir 'renderdoc.err.log')
            $proc.WaitForExit()
            Clear-ProjectVBenchmarkEnv
            $summary.exitCode = $proc.ExitCode
            $summary.artifacts.renderDocLog = $rdLog
            $rdc = Get-ChildItem -LiteralPath $OutDir -Filter '*.rdc' -ErrorAction SilentlyContinue
            if ($rdc) {
                foreach ($f in $rdc) { $summary.artifacts[$f.Name] = $f.FullName }
            } else {
                $summary.notes.Add('No .rdc written - F12 was not pressed (or capture failed).')
                $summary.status = 'failed'
                Write-ProfileSummary -Summary $summary
                exit 3
            }
        }
    }

    $summary.benchmark = Parse-BenchmarkLine -LogPath $appLog
    if (-not $summary.benchmark.raw) {
        # Some tools redirect their own logs; also scan OutDir logs.
        Get-ChildItem -LiteralPath $OutDir -Filter '*.log' -ErrorAction SilentlyContinue | ForEach-Object {
            $parsed = Parse-BenchmarkLine -LogPath $_.FullName
            if ($parsed.raw) { $summary.benchmark = $parsed }
        }
    }

    if ($summary.status -ne 'failed') {
        $summary.status = 'ok'
    }
    Write-ProfileSummary -Summary $summary
    Write-Host "profile: PASS - $summaryPath"
    Write-Host "profile: digest - $summaryMdPath"
    if ($summary.benchmark.mean_ms) {
        Write-Host ("profile: benchmark mean_ms={0} mean_fps={1}" -f $summary.benchmark.mean_ms, $summary.benchmark.mean_fps)
    }
    exit 0
} catch {
    Clear-ProjectVBenchmarkEnv
    $summary.status = 'failed'
    $summary.notes.Add("$_")
    Write-ProfileSummary -Summary $summary
    Write-Error $_
    exit 3
}
