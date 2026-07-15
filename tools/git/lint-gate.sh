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

mapfile -t FILES < <(collect_files)

if [[ ${#FILES[@]} -eq 0 ]]; then
	echo "lint-gate: no lintable C/C++ files (${MODE}) — OK"
	exit 0
fi

echo "lint-gate: mode=${MODE} build=${BUILD_DIR} files=${#FILES[@]}"
printf '  %s\n' "${FILES[@]}"

echo "lint-gate: clang-format -i ..."
clang-format -i --style=file "${FILES[@]}"

if [[ "${MODE}" == "commit" ]]; then
	git add -- "${FILES[@]}"
fi

echo "lint-gate: clang-format --dry-run --Werror ..."
clang-format --dry-run --Werror --style=file "${FILES[@]}"

TIDY_FILES=()
for f in "${FILES[@]}"; do
	case "${f}" in
		*.cpp | *.cc | *.cxx | *.c) TIDY_FILES+=("${f}") ;;
	esac
done

if [[ ${#TIDY_FILES[@]} -eq 0 ]]; then
	echo "lint-gate: no TU sources for clang-tidy — format OK"
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
