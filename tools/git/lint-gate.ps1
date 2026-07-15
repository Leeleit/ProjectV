# ProjectV lint gate (native PowerShell): auto clang-format + clang-tidy Werror.
# Usage: .\tools\git\lint-gate.ps1 [commit|push|working]
# Bypass: $env:PROJECTV_SKIP_LINT = "1"
param(
	[ValidateSet("commit", "push", "working")]
	[string]$Mode = "working"
)

$ErrorActionPreference = "Stop"
if ($env:PROJECTV_SKIP_LINT -eq "1") {
	Write-Host "lint-gate: skipped (PROJECTV_SKIP_LINT=1)"
	exit 0
}

$Root = git rev-parse --show-toplevel
Set-Location $Root

function Die([string]$Message) {
	Write-Error "lint-gate: ERROR: $Message"
	exit 1
}

foreach ($cmd in @("clang-format", "clang-tidy")) {
	if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
		Die "'$cmd' not found in PATH (install LLVM clang-format/clang-tidy)"
	}
}

function Resolve-BuildDir {
	$candidates = @()
	if ($env:PROJECTV_BUILD_DIR) { $candidates += $env:PROJECTV_BUILD_DIR }
	$candidates += @(
		"build/windows-clang-debug",
		"build/linux-clang-debug",
		"build/linux-clang-debug-ci",
		"build/windows-clang-debug-ci",
		"build/windows-clang-release",
		"build/linux-clang-release"
	)
	foreach ($c in $candidates) {
		if (Test-Path (Join-Path $c "compile_commands.json")) { return $c }
	}
	return $null
}

$BuildDir = Resolve-BuildDir
if (-not $BuildDir) {
	Die "compile_commands.json not found; configure a preset first or set PROJECTV_BUILD_DIR"
}

function Test-Lintable([string]$Path) {
	$norm = $Path.Replace("\", "/")
	if ($norm -match '^(external|build|legacy|\.git)/') { return $false }
	if ($norm -notmatch '^(src|tests|tools)/') { return $false }
	return $norm -match '\.(cpp|cc|cxx|c|hpp|hh|h|ixx)$'
}

function Get-ChangedFiles {
	$raw = switch ($Mode) {
		"commit" { git diff --cached --name-only --diff-filter=ACMR }
		"working" { git diff --name-only --diff-filter=ACMR HEAD }
		"push" {
			$upstream = $null
			try { $upstream = git rev-parse --abbrev-ref --symbolic-full-name "@{upstream}" 2>$null } catch { }
			if ($upstream) { git diff --name-only --diff-filter=ACMR "@{upstream}...HEAD" }
			else { git diff --name-only --diff-filter=ACMR HEAD }
		}
	}
	$raw | Where-Object { $_ -and (Test-Path $_) -and (Test-Lintable $_) } | Sort-Object -Unique
}

function Get-CompileDbFiles {
	python -c @"
import json
from pathlib import Path
build = Path(r'$BuildDir')
root = Path('.').resolve()
data = json.loads((build / 'compile_commands.json').read_text(encoding='utf-8'))
out = set()
for e in data:
    f = Path(e.get('file', ''))
    if not f.is_absolute():
        f = (Path(e.get('directory', build)) / f)
    try:
        rel = f.resolve().relative_to(root).as_posix()
    except Exception:
        rel = f.as_posix().replace('\\\\', '/')
        marker = '/ProjectV/'
        if marker in rel:
            rel = rel.split(marker, 1)[1]
    out.add(rel)
print('\n'.join(sorted(out)))
"@
}

$Files = @(Get-ChangedFiles)
if ($Files.Count -eq 0) {
	Write-Host "lint-gate: no lintable C/C++ files ($Mode) — OK"
	exit 0
}

Write-Host "lint-gate: mode=$Mode build=$BuildDir files=$($Files.Count)"
$Files | ForEach-Object { Write-Host "  $_" }

if ($Mode -eq "push") {
	Write-Host "lint-gate: clang-format --dry-run --Werror ..."
	& clang-format --dry-run --Werror --style=file @Files
	if ($LASTEXITCODE -ne 0) { Die "clang-format check failed" }
} else {
	Write-Host "lint-gate: clang-format -i ..."
	& clang-format -i --style=file @Files
	if ($LASTEXITCODE -ne 0) { Die "clang-format -i failed" }
	if ($Mode -eq "commit") {
		git add -- @Files
	}
	Write-Host "lint-gate: clang-format --dry-run --Werror ..."
	& clang-format --dry-run --Werror --style=file @Files
	if ($LASTEXITCODE -ne 0) { Die "clang-format check failed" }
}

$compileDb = @{}
Get-CompileDbFiles | ForEach-Object { if ($_) { $compileDb[$_] = $true } }

$TidyFiles = @()
$SkippedNoDb = @()
foreach ($f in $Files) {
	$norm = $f.Replace("\", "/")
	if ($norm -notmatch '\.(cpp|cc|cxx|c)$') { continue }
	if ($compileDb.ContainsKey($norm)) { $TidyFiles += $norm }
	else { $SkippedNoDb += $norm }
}

if ($SkippedNoDb.Count -gt 0) {
	Write-Host "lint-gate: WARNING: not in compile_commands.json (target disabled/not built in this preset — tidy skipped):"
	$SkippedNoDb | ForEach-Object { Write-Host "  $_" }
}

if ($TidyFiles.Count -eq 0) {
	Write-Host "lint-gate: no TUs in compile_commands for clang-tidy — format OK"
	exit 0
}

Write-Host "lint-gate: clang-tidy --warnings-as-errors=* ($($TidyFiles.Count) TUs) ..."
$tidyFail = $false
foreach ($f in $TidyFiles) {
	$out = & clang-tidy $f -p $BuildDir --warnings-as-errors='*' --quiet 2>&1
	$code = $LASTEXITCODE
	if ($code -ne 0) {
		Write-Host ($out | Out-String)
		Write-Host "lint-gate: clang-tidy FAILED: $f (exit $code)" -ForegroundColor Red
		$tidyFail = $true
	}
}

if ($tidyFail) {
	Die "clang-tidy reported warnings/errors — commit/push rejected"
}

Write-Host "lint-gate: OK"
exit 0
