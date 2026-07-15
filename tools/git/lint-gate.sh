#!/usr/bin/env bash
# ProjectV lint gate: auto clang-format + clang-tidy (warnings-as-errors).
# Usage: lint-gate.sh <commit|push|working>
# Bypass (operator only): PROJECTV_SKIP_LINT=1
set -euo pipefail

MODE="${1:-working}"
ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "${ROOT_DIR}"

if [[ "${PROJECTV_SKIP_LINT:-}" == "1" ]]; then
	echo "lint-gate: skipped (PROJECTV_SKIP_LINT=1)"
	exit 0
fi

die() {
	echo "lint-gate: ERROR: $*" >&2
	exit 1
}

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || die "'$1' not found in PATH (install LLVM clang-format/clang-tidy)"
}

need_cmd clang-format
need_cmd clang-tidy

resolve_build_dir() {
	local candidates=()
	if [[ -n "${PROJECTV_BUILD_DIR:-}" ]]; then
		candidates+=("${PROJECTV_BUILD_DIR}")
	fi
	candidates+=(
		"build/windows-clang-debug"
		"build/linux-clang-debug"
		"build/linux-clang-debug-ci"
		"build/windows-clang-debug-ci"
		"build/windows-clang-release"
		"build/linux-clang-release"
	)
	local c
	for c in "${candidates[@]}"; do
		if [[ -f "${c}/compile_commands.json" ]]; then
			printf '%s\n' "${c}"
			return 0
		fi
	done
	return 1
}

BUILD_DIR="$(resolve_build_dir)" || die "compile_commands.json not found; configure a preset first (CMAKE_EXPORT_COMPILE_COMMANDS=ON) or set PROJECTV_BUILD_DIR"

is_lintable() {
	local f="$1"
	case "${f}" in
		external/* | .git/* | build/* | legacy/*) return 1 ;;
	esac
	case "${f}" in
		src/* | tests/* | tools/*) ;;
		*) return 1 ;;
	esac
	case "${f}" in
		*.cpp | *.cc | *.cxx | *.c | *.hpp | *.hh | *.h | *.ixx) return 0 ;;
		*) return 1 ;;
	esac
}

list_from_lines() {
	while IFS= read -r f; do
		[[ -z "${f}" ]] && continue
		[[ -f "${f}" ]] || continue
		is_lintable "${f}" && printf '%s\n' "${f}"
	done | sort -u
}

collect_push_names() {
	local local_ref local_sha remote_ref remote_sha range
	local got=0
	if [[ ! -t 0 ]]; then
		while read -r local_ref local_sha remote_ref remote_sha; do
			[[ -z "${local_sha:-}" ]] && continue
			[[ "${local_sha}" =~ ^0+$ ]] && continue
			if [[ "${remote_sha}" =~ ^0+$ ]]; then
				range="${local_sha}"
			else
				range="${remote_sha}..${local_sha}"
			fi
			git diff --name-only --diff-filter=ACMR "${range}" || true
			got=1
		done
	fi
	if [[ "${got}" -eq 0 ]]; then
		if git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' >/dev/null 2>&1; then
			git diff --name-only --diff-filter=ACMR '@{upstream}...HEAD' || true
		else
			git diff --name-only --diff-filter=ACMR HEAD || true
		fi
	fi
}

collect_files() {
	case "${MODE}" in
		commit)
			git diff --cached --name-only --diff-filter=ACMR | list_from_lines
			;;
		working)
			git diff --name-only --diff-filter=ACMR HEAD | list_from_lines
			;;
		push)
			collect_push_names | list_from_lines
			;;
		*)
			die "unknown mode '${MODE}' (use commit|push|working)"
			;;
	esac
}

# Normalize compile_commands paths to repo-relative forward-slash form.
load_compile_db_files() {
	python - "${BUILD_DIR}" <<'PY'
import json, os, sys
from pathlib import Path
build = Path(sys.argv[1])
root = Path(".").resolve()
data = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
out = set()
for e in data:
    f = Path(e.get("file", ""))
    if not f.is_absolute():
        f = (Path(e.get("directory", build)) / f)
    try:
        rel = f.resolve().relative_to(root).as_posix()
    except Exception:
        rel = f.as_posix().replace("\\", "/")
        # Strip drive + repo prefix if present
        marker = "/ProjectV/"
        if marker in rel:
            rel = rel.split(marker, 1)[1]
    out.add(rel)
for r in sorted(out):
    print(r)
PY
}

mapfile -t FILES < <(collect_files)

if [[ ${#FILES[@]} -eq 0 ]]; then
	echo "lint-gate: no lintable C/C++ files (${MODE}) — OK"
	exit 0
fi

echo "lint-gate: mode=${MODE} build=${BUILD_DIR} files=${#FILES[@]}"
printf '  %s\n' "${FILES[@]}"

# Format: auto-fix on commit/working; dry-run only on push (no rewrite of pushed history).
if [[ "${MODE}" == "push" ]]; then
	echo "lint-gate: clang-format --dry-run --Werror ..."
	clang-format --dry-run --Werror --style=file "${FILES[@]}"
else
	echo "lint-gate: clang-format -i ..."
	clang-format -i --style=file "${FILES[@]}"
	if [[ "${MODE}" == "commit" ]]; then
		git add -- "${FILES[@]}"
	fi
	echo "lint-gate: clang-format --dry-run --Werror ..."
	clang-format --dry-run --Werror --style=file "${FILES[@]}"
fi

mapfile -t COMPILE_DB_FILES < <(load_compile_db_files)
declare -A IN_COMPILE_DB=()
for f in "${COMPILE_DB_FILES[@]}"; do
	f="${f%$'\r'}" # Windows Python may emit CRLF into the pipe
	[[ -n "${f}" ]] || continue
	IN_COMPILE_DB["${f}"]=1
done

TIDY_FILES=()
SKIPPED_NO_DB=()
for f in "${FILES[@]}"; do
	f="${f%$'\r'}"
	case "${f}" in
		*.cpp | *.cc | *.cxx | *.c) ;;
		*) continue ;;
	esac
	if [[ -n "${IN_COMPILE_DB[${f}]+x}" ]]; then
		TIDY_FILES+=("${f}")
	else
		SKIPPED_NO_DB+=("${f}")
	fi
done

if [[ ${#SKIPPED_NO_DB[@]} -gt 0 ]]; then
	echo "lint-gate: WARNING: not in compile_commands.json (target disabled/not built in this preset — tidy skipped):"
	printf '  %s\n' "${SKIPPED_NO_DB[@]}"
fi

if [[ ${#TIDY_FILES[@]} -eq 0 ]]; then
	echo "lint-gate: no TUs in compile_commands for clang-tidy — format OK"
	exit 0
fi

echo "lint-gate: clang-tidy --warnings-as-errors=* (${#TIDY_FILES[@]} TUs) ..."
TIDY_FAIL=0
for f in "${TIDY_FILES[@]}"; do
	# clang-tidy 22 may still print "Processing file" progress under --quiet; only exit code matters (--warnings-as-errors=*).
	if ! out="$(clang-tidy "${f}" -p "${BUILD_DIR}" --warnings-as-errors='*' --quiet 2>&1)"; then
		printf '%s\n' "${out}"
		echo "lint-gate: clang-tidy FAILED: ${f}" >&2
		TIDY_FAIL=1
	fi
done

if [[ "${TIDY_FAIL}" -ne 0 ]]; then
	die "clang-tidy reported warnings/errors — commit/push rejected"
fi

echo "lint-gate: OK"
exit 0
