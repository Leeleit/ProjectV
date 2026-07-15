#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "${ROOT_DIR}"

chmod +x \
	"${ROOT_DIR}/tools/git/lint-gate.sh" \
	"${ROOT_DIR}/tools/git/pre-commit" \
	"${ROOT_DIR}/tools/git/pre-push" \
	"${ROOT_DIR}/tools/git/install-hooks.sh"

# Prefer core.hooksPath (works on Windows + Linux without symlinks).
git config core.hooksPath tools/git

echo "Installed git hooks via core.hooksPath=tools/git"
echo "  pre-commit → lint-gate (auto clang-format + clang-tidy Werror)"
echo "  pre-push   → lint-gate + Docker CI (if docker available)"
echo
echo "Bypass (operator only): PROJECTV_SKIP_LINT=1 / PROJECTV_SKIP_DOCKER_CI=1"
echo "Manual run: tools/git/lint-gate.sh working"
