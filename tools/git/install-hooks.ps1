# Install ProjectV git hooks (Windows PowerShell).
# Sets core.hooksPath=tools/git so pre-commit/pre-push run from the repo.
$ErrorActionPreference = "Stop"
$Root = git rev-parse --show-toplevel
Set-Location $Root

$prev = git config --get core.hooksPath
git config core.hooksPath tools/git

Write-Host "Installed git hooks via core.hooksPath=tools/git (was: $(if ($prev) { $prev } else { '<unset>' }))"
Write-Host "  pre-commit -> lint-gate (auto clang-format + clang-tidy Werror)"
Write-Host "  pre-push   -> lint-gate + Docker CI (if docker available)"
Write-Host ""
Write-Host "Bypass (operator only): `$env:PROJECTV_SKIP_LINT=1 / `$env:PROJECTV_SKIP_DOCKER_CI=1"
Write-Host "Manual run: bash tools/git/lint-gate.sh working"
Write-Host "         or: .\tools\git\lint-gate.ps1 working"
Write-Host ""
Write-Host "Note: Git hooks need Git's bash (scoop/Git for Windows). Ensure LLVM clang-format/clang-tidy are on PATH."
