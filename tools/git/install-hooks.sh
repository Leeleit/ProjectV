#!/bin/bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
HOOKS_DIR="${ROOT_DIR}/.git/hooks"

mkdir -p "${HOOKS_DIR}"

ln -sf "${ROOT_DIR}/tools/git/pre-commit" "${HOOKS_DIR}/pre-commit"
ln -sf "${ROOT_DIR}/tools/git/pre-push" "${HOOKS_DIR}/pre-push"

echo "Installed git hooks:"
ls -l "${HOOKS_DIR}/pre-commit" "${HOOKS_DIR}/pre-push"
