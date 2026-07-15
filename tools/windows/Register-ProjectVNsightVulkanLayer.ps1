# Register-ProjectVNsightVulkanLayer.ps1
# One-time elevated helper: lets nsys write the Vulkan implicit layer into HKLM
# so subsequent non-admin `Invoke-ProjectVProfile.ps1 -Tool Systems` can use
# --trace=vulkan without "registry writing permissions" failures.
#
# Usage (UAC prompt expected):
#   powershell -ExecutionPolicy Bypass -File tools\windows\Register-ProjectVNsightVulkanLayer.ps1

#Requires -RunAsAdministrator
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\Resolve-ProjectVProfilerTools.ps1"
$tools = Resolve-ProjectVProfilerTools
if (-not $tools.Nsys) {
    throw 'nsys.exe not found'
}

$layerJson = 'C:\Program Files\NVIDIA Corporation\Nsight Systems 2026.3.1\target-windows-x64\vulkan-layers\VkLayer_nsight-sys_windows.json'
if (-not (Test-Path -LiteralPath $layerJson)) {
    throw "Layer JSON missing: $layerJson"
}

$regPath = 'HKLM:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
if (-not (Test-Path $regPath)) {
    New-Item -Path $regPath -Force | Out-Null
}
New-ItemProperty -Path $regPath -Name $layerJson -PropertyType DWord -Value 0 -Force | Out-Null
Write-Host "Registered: $layerJson under $regPath"

# Smoke a 1s vulkan profile to confirm nsys accepts --trace=vulkan without registry errors.
$tmp = Join-Path $env:TEMP ("projectv-nsys-register-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
try {
    $probeOut = Join-Path $tmp 'probe'
    & $tools.Nsys @(
        'profile'
        '--force-overwrite=true'
        "--output=$probeOut"
        '--trace=vulkan,nvtx'
        '--duration=1'
        '--kill=true'
        '--sample=none'
        '--cpuctxsw=none'
        "$env:WINDIR\System32\cmd.exe"
        '/c'
        'exit 0'
    ) 2>&1 | Out-Host
    Write-Host 'Register OK. Re-run Invoke-ProjectVProfile.ps1 -Tool Systems for full Vulkan timelines.'
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
