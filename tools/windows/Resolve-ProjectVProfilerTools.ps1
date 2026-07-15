# Resolve-ProjectVProfilerTools.ps1 — locate Nsight + RenderDoc CLIs on this host.
# Dot-source from other profiling scripts; does not mutate PATH.
#
# Override any path via env:
#   PROJECTV_NSYS, PROJECTV_NCU, PROJECTV_NGFX, PROJECTV_NGFX_CAPTURE,
#   PROJECTV_NGFX_REPLAY, PROJECTV_RENDERDOC_CMD

Set-StrictMode -Version Latest

function Find-FirstExistingPath {
    param([Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path -LiteralPath $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

function Resolve-ProjectVProfilerTools {
    $nsightRoot = 'C:\Program Files\NVIDIA Corporation'
    $ngfxHost = Join-Path $nsightRoot 'Nsight Graphics 2026.2.0\host\windows-desktop-nomad-x64'

    function Select-NonEmpty {
        param([object[]]$Items)
        $out = @()
        foreach ($item in $Items) {
            if ($null -ne $item -and -not [string]::IsNullOrWhiteSpace([string]$item)) {
                $out += [string]$item
            }
        }
        return ,$out
    }

    $nsysCandidates = Select-NonEmpty @(
        $env:PROJECTV_NSYS
        (Join-Path $nsightRoot 'Nsight Systems 2026.3.1\target-windows-x64\nsys.exe')
        (Join-Path $nsightRoot 'Nsight Compute 2026.2.1\host\target-windows-x64\nsys.exe')
    )
    $ncuCandidates = Select-NonEmpty @(
        $env:PROJECTV_NCU
        (Join-Path $nsightRoot 'Nsight Compute 2026.2.1\target\windows-desktop-win7-x64\ncu.exe')
    )
    $ngfxCandidates = Select-NonEmpty @(
        $env:PROJECTV_NGFX
        (Join-Path $ngfxHost 'ngfx.exe')
    )
    $ngfxCaptureCandidates = Select-NonEmpty @(
        $env:PROJECTV_NGFX_CAPTURE
        (Join-Path $ngfxHost 'ngfx-capture.exe')
    )
    $ngfxReplayCandidates = Select-NonEmpty @(
        $env:PROJECTV_NGFX_REPLAY
        (Join-Path $ngfxHost 'ngfx-replay.exe')
    )
    $renderDocCandidates = Select-NonEmpty @(
        $env:PROJECTV_RENDERDOC_CMD
        'C:\Program Files\RenderDoc\renderdoccmd.exe'
    )

    return [pscustomobject]@{
        Nsys            = Find-FirstExistingPath -Candidates $nsysCandidates
        Ncu             = Find-FirstExistingPath -Candidates $ncuCandidates
        Ngfx            = Find-FirstExistingPath -Candidates $ngfxCandidates
        NgfxCapture     = Find-FirstExistingPath -Candidates $ngfxCaptureCandidates
        NgfxReplay      = Find-FirstExistingPath -Candidates $ngfxReplayCandidates
        RenderDocCmd    = Find-FirstExistingPath -Candidates $renderDocCandidates
        GpuArchitecture = if ($env:PROJECTV_NSIGHT_GPU_ARCH) { $env:PROJECTV_NSIGHT_GPU_ARCH } else { 'Ampere GA10x' }
    }
}
