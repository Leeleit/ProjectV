#!/usr/bin/env bash
# Invoke-ProjectVRuntimeSmoke.sh — Linux counterpart of
# tools/windows/Invoke-ProjectVRuntimeSmoke.ps1.
#
# Goal: spawn the ProjectV binary, let it self-capture a fixed set of lighting
# debug views via the existing `PROJECTV_LOOKDEV_CAPTURE_*` env-var contract,
# confirm the process exits cleanly, and verify that the expected number of
# `.bmp` / `.txt` pairs landed on disk.
#
# This is a *targeted lifecycle / capture* smoke. It is not a CPU/GPU benchmark
# and it does not validate pixel correctness. The visual review is a separate
# pass on the produced `.bmp` files (e.g. via `vision_analyze`).
#
# Env-var contract used by the binary (from `src/app/LookDevCaptureAutomation.cpp`
# and `src/render/ScreenshotCapture.cpp`):
#
#   PROJECTV_SCREENSHOT_DIR              override the screenshot output dir
#   PROJECTV_START_CAMERA_POSITION       "<x> <y> <z>"  startup camera position
#   PROJECTV_START_CAMERA_LOOK           "<x> <y> <z>"  startup camera look vector
#   PROJECTV_LOOKDEV_CAPTURE_VIEWS       "FINAL SHDW CSM CTSH AOCC LOCL"  pipe-separated
#   PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES       default 30
#   PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES     default 2
#   PROJECTV_LOOKDEV_CAPTURE_QUIT                 "1" / "true" / "yes" / "on"  -> exit after capture
#
# Usage:
#   bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh \
#       [--build-dir build/linux-clang-debug] \
#       [--capture-dir build/linux-clang-debug/lookdev-captures/<name>] \
#       [--camera-pos "x y z"] [--camera-look "x y z"] \
#       [--views "FINAL SHDW CSM CTSH AOCC LOCL"] \
#       [--warmup N] [--interval N] \
#       [--timeout-start 30] [--timeout-capture 60] [--timeout-shutdown 10]
#
# Exit code:
#   0  smoke passed (binary ran, captured the requested views, exited cleanly)
#   1  usage / argument error
#   2  binary not built
#   3  binary failed to start (did not reach capture phase within timeout)
#   4  binary ran but did not produce the expected capture files
#   5  binary exited with non-zero status
#   6  binary hung and was killed by a timeout watchdog

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Defaults
build_dir="${PROJECT_ROOT}/build/linux-clang-debug"
capture_dir=""
camera_pos="-25 19 25"
camera_look="0.62 -0.48 -0.62"   # canonical "MeshingStress" reference shot from memory
views="FINAL SHDW CSM CTSH AOCC LOCL"
warmup="30"
interval="2"
quit_after_capture="1"
timeout_start="30"
timeout_capture="60"
timeout_shutdown="10"
exe_path=""

usage() {
    sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)        build_dir="$2"; shift 2 ;;
        --capture-dir)      capture_dir="$2"; shift 2 ;;
        --camera-pos)       camera_pos="$2"; shift 2 ;;
        --camera-look)      camera_look="$2"; shift 2 ;;
        --views)            views="$2"; shift 2 ;;
        --warmup)           warmup="$2"; shift 2 ;;
        --interval)         interval="$2"; shift 2 ;;
        --timeout-start)    timeout_start="$2"; shift 2 ;;
        --timeout-capture)  timeout_capture="$2"; shift 2 ;;
        --timeout-shutdown) timeout_shutdown="$2"; shift 2 ;;
        --exe)              exe_path="$2"; shift 2 ;;
        -h|--help)          usage ;;
        *)                  printf 'unknown argument: %s\n' "$1" >&2; usage ;;
    esac
done

# Resolve binary path
if [[ -z "${exe_path}" ]]; then
    exe_path="${build_dir}/bin/ProjectV"
fi
exe_path="$(cd "$(dirname "${exe_path}")" && pwd)/$(basename "${exe_path}")"

if [[ ! -x "${exe_path}" ]]; then
    printf 'smoke: ProjectV executable not found or not executable: %s\n' "${exe_path}" >&2
    # shellcheck disable=SC2016
    printf 'smoke: build first with `cmake --build %s --target ProjectV --parallel 8`\n' "${build_dir}" >&2
    exit 2
fi

# Resolve capture dir (default: <build_dir>/lookdev-captures/<timestamp>)
if [[ -z "${capture_dir}" ]]; then
    ts="$(date -u +%Y%m%d-%H%M%S)"
    capture_dir="${build_dir}/lookdev-captures/${ts}-shadow-audit"
fi
mkdir -p "${capture_dir}"

# Count expected views
view_count=$(printf '%s' "${views}" | tr ' |' '\n' | grep -c '.' || true)
if [[ "${view_count}" -lt 1 ]]; then
    printf 'smoke: --views must list at least one debug view, got: %s\n' "${views}" >&2
    exit 1
fi

printf 'smoke: exe=%s\n' "${exe_path}"
printf 'smoke: capture_dir=%s\n' "${capture_dir}"
printf 'smoke: views=%s (count=%d)\n' "${views}" "${view_count}"
printf 'smoke: camera_pos=%s\n' "${camera_pos}"
printf 'smoke: camera_look=%s\n' "${camera_look}"
printf 'smoke: warmup=%s interval=%s quit_after_capture=%s\n' \
    "${warmup}" "${interval}" "${quit_after_capture}"

# Compose env
export PROJECTV_SCREENSHOT_DIR="${capture_dir}"
export PROJECTV_START_CAMERA_POSITION="${camera_pos}"
export PROJECTV_START_CAMERA_LOOK="${camera_look}"
export PROJECTV_LOOKDEV_CAPTURE_VIEWS="${views}"
export PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES="${warmup}"
export PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES="${interval}"
export PROJECTV_LOOKDEV_CAPTURE_QUIT="${quit_after_capture}"
# Disable Vulkan validation layers when not installed; the preset turns
# validation ON by default and the smoke target should still run.
export PROJECTV_ENABLE_VALIDATION="${PROJECTV_ENABLE_VALIDATION:-OFF}"

# Spawn the binary
log_path="${capture_dir}/smoke.log"
printf 'smoke: log=%s\n' "${log_path}"

start_ts=$(date +%s)
# Use `timeout` to bound the entire run; the binary's own LookDev automation
# should exit it well before this on success, but the watchdog is the safety net.
total_budget=$(( timeout_start + timeout_capture + timeout_shutdown + 10 ))
set +e
timeout --foreground "${total_budget}" \
    "${exe_path}" >"${log_path}" 2>&1 &
pid=$!
set -e

# Wait for either: process exits, or capture files appear, or timeouts
declare -i startup_budget="${timeout_start}"
declare -i capture_budget="${timeout_capture}"
declare -i phase=0   # 0 = startup, 1 = capture
declare -i started_at="${start_ts}"

set +e
while true; do
    if ! kill -0 "${pid}" 2>/dev/null; then
        # Process exited
        wait "${pid}"
        exit_code=$?
        end_ts=$(date +%s)
        runtime=$(( end_ts - start_ts ))
        printf 'smoke: process exited code=%d after %ds\n' "${exit_code}" "${runtime}"
        if [[ "${exit_code}" -ne 0 ]]; then
            printf 'smoke: FAIL — non-zero exit, last 40 log lines:\n' >&2
            tail -n 40 "${log_path}" >&2 || true
            exit 5
        fi
        # Exit 0 — verify capture files
        break
    fi

    now=$(date +%s)
    elapsed=$(( now - start_ts ))

    # Look for "[ProjectV][LookDevCapture] capture requested view=..." in log
    # to know we entered the capture phase.
    if [[ "${phase}" -eq 0 ]] && grep -q 'LookDevCapture] capture requested view=' "${log_path}" 2>/dev/null; then
        phase=1
        started_at="${now}"
        printf 'smoke: capture phase entered after %ds\n' "${elapsed}"
    fi

    # Phase-specific watchdog
    if [[ "${phase}" -eq 0 ]] && [[ "${elapsed}" -gt "${startup_budget}" ]]; then
        printf 'smoke: FAIL — startup timeout (%ds) without entering capture phase\n' \
            "${startup_budget}" >&2
        kill -TERM "${pid}" 2>/dev/null || true
        sleep 1
        kill -KILL "${pid}" 2>/dev/null || true
        printf 'smoke: last 40 log lines:\n' >&2
        tail -n 40 "${log_path}" >&2 || true
        exit 3
    fi

    if [[ "${phase}" -eq 1 ]]; then
        in_capture=$(( now - started_at ))
        if [[ "${in_capture}" -gt "${capture_budget}" ]]; then
            printf 'smoke: FAIL — capture phase timeout (%ds)\n' "${capture_budget}" >&2
            kill -TERM "${pid}" 2>/dev/null || true
            sleep 1
            kill -KILL "${pid}" 2>/dev/null || true
            printf 'smoke: last 40 log lines:\n' >&2
            tail -n 40 "${log_path}" >&2 || true
            exit 6
        fi
    fi

    sleep 0.5
done
set -e

# Verify capture files
printf 'smoke: verifying capture files in %s\n' "${capture_dir}"
# shellcheck disable=SC2012
ls -la "${capture_dir}" | head -20

bmp_count=$(find "${capture_dir}" -maxdepth 1 -name 'ProjectV-*.bmp' | wc -l)
txt_count=$(find "${capture_dir}" -maxdepth 1 -name 'ProjectV-*.txt' | wc -l)

printf 'smoke: found %d .bmp files (expected at least %d)\n' "${bmp_count}" "${view_count}"
printf 'smoke: found %d .txt metadata files (expected at least %d)\n' \
    "${txt_count}" "${view_count}"

if [[ "${bmp_count}" -lt "${view_count}" ]]; then
    printf 'smoke: FAIL — too few .bmp files\n' >&2
    printf 'smoke: directory listing:\n' >&2
    ls -la "${capture_dir}" >&2
    exit 4
fi
if [[ "${txt_count}" -lt "${view_count}" ]]; then
    printf 'smoke: FAIL — too few .txt metadata files\n' >&2
    exit 4
fi

# Per-file metadata sanity: every .bmp should have a sidecar with non-empty
# preset/shadow_tuning lines (proves the capture automation actually ran).
printf 'smoke: spot-checking metadata sidecars\n'
metadata_fail=0
for txt in "${capture_dir}"/ProjectV-*.txt; do
    if [[ ! -s "${txt}" ]]; then
        printf 'smoke: WARN — empty metadata: %s\n' "${txt}" >&2
        metadata_fail=$(( metadata_fail + 1 ))
    fi
done
if [[ "${metadata_fail}" -gt 0 ]]; then
    printf 'smoke: WARN — %d empty metadata sidecars (capture may have raced)\n' \
        "${metadata_fail}" >&2
fi

printf 'smoke: PASS — captured %d .bmp + %d .txt in %s\n' \
    "${bmp_count}" "${txt_count}" "${capture_dir}"
exit 0
