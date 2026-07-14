#!/usr/bin/env bash
# Invoke-ProjectVTracyCapture.sh — headless Tracy capture for ProjectV.
#
# Goal: run ProjectV with benchmark automation so it auto-quits after a fixed
# frame budget, connect tracy-capture to it, and save the resulting .tracy trace.
#
# ProjectV uses SDL_MAIN_USE_CALLBACKS, so argv is intentionally ignored by
# SDL_AppInit. Do NOT pass `--smoke` or similar flags to the binary; use the
# env-var contract instead.
#
# tracy-capture does NOT launch the application; it only connects to an already
# listening Tracy server. Therefore this script starts tracy-capture first, then
# launches ProjectV, and waits for both processes to finish.
#
# Env-var contract used by the binary (from src/app/BenchmarkAutomation.cpp):
#
#   PROJECTV_BENCHMARK_FRAMES          number of frames to measure (required)
#   PROJECTV_BENCHMARK_WARMUP_FRAMES   default 30
#   PROJECTV_BENCHMARK_QUIT            "1" / "true" / "yes" / "on" -> exit after measurement
#
# Usage:
#   bash tools/linux/Invoke-ProjectVTracyCapture.sh \
#       [--build-dir build/linux-clang-debug] \
#       [--output build/tracy-captures/phase3.tracy] \
#       [--frames 120] [--warmup 30] \
#       [--timeout 180] \
#       [--validation ON|OFF]
#
# Exit code:
#   0  capture succeeded and .tracy file was written
#   1  usage error
#   2  binary or tracy-capture not found
#   3  ProjectV failed to start
#   4  tracy-capture failed or timed out
#   5  ProjectV exited with non-zero status

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

build_dir="${PROJECT_ROOT}/build/linux-clang-debug"
output_path=""
frames="120"
warmup="30"
total_timeout="180"
validation="OFF"
tracy_capture="${PROJECT_ROOT}/build/tracy-capture-cli/tracy-capture"
tracy_port="8086"

usage() {
    sed -n '2,36p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)      build_dir="$2"; shift 2 ;;
        --output)         output_path="$2"; shift 2 ;;
        --frames)         frames="$2"; shift 2 ;;
        --warmup)         warmup="$2"; shift 2 ;;
        --timeout)        total_timeout="$2"; shift 2 ;;
        --validation)     validation="$2"; shift 2 ;;
        --tracy-capture)  tracy_capture="$2"; shift 2 ;;
        -h|--help)        usage ;;
        *)                printf 'unknown argument: %s\n' "$1" >&2; usage ;;
    esac
done

exe_path="${build_dir}/bin/ProjectV"
if [[ ! -x "${exe_path}" ]]; then
    printf 'tracy-capture: ProjectV executable not found or not executable: %s\n' "${exe_path}" >&2
    printf 'tracy-capture: build first with `cmake --build %s --target ProjectV --parallel 8`\n' "${build_dir}" >&2
    exit 2
fi

if [[ ! -x "${tracy_capture}" ]]; then
    printf 'tracy-capture: tracy-capture binary not found: %s\n' "${tracy_capture}" >&2
    printf 'tracy-capture: build it first (see tools/tracy-standalone/capture/CMakeLists.txt)\n' >&2
    exit 2
fi

if [[ -z "${output_path}" ]]; then
    mkdir -p "${build_dir}/tracy-captures"
    output_path="${build_dir}/tracy-captures/$(date -u +%Y%m%d-%H%M%S)-tracy.tracy"
fi
output_dir="$(dirname "${output_path}")"
mkdir -p "${output_dir}"
output_path="$(cd "${output_dir}" && pwd)/$(basename "${output_path}")"

printf 'tracy-capture: exe=%s\n' "${exe_path}"
printf 'tracy-capture: output=%s\n' "${output_path}"
printf 'tracy-capture: frames=%s warmup=%s validation=%s timeout=%s\n' \
    "${frames}" "${warmup}" "${validation}" "${total_timeout}"

export PROJECTV_BENCHMARK_FRAMES="${frames}"
export PROJECTV_BENCHMARK_WARMUP_FRAMES="${warmup}"
export PROJECTV_BENCHMARK_QUIT="1"
export PROJECTV_ENABLE_VALIDATION="${validation}"

# Start tracy-capture first so it is ready the instant ProjectV opens its socket.
set +e
"${tracy_capture}" -f -o "${output_path}" -p "${tracy_port}" -s "${total_timeout}" >"${output_path}.log" 2>&1 &
capture_pid=$!
set -e

printf 'tracy-capture: capture pid=%d\n' "${capture_pid}"

# Give tracy-capture a moment to enter its connect loop.
sleep 0.5

# Launch ProjectV. It will auto-quit after the benchmark frame budget.
set +e
"${exe_path}" >/tmp/projectv-tracy-capture.log 2>&1 &
projectv_pid=$!
set -e

printf 'tracy-capture: ProjectV pid=%d\n' "${projectv_pid}"

# Wait for ProjectV to finish.
wait "${projectv_pid}"
projectv_exit=$?

printf 'tracy-capture: ProjectV exited with code %d\n' "${projectv_exit}"

# ProjectV has exited; tracy-capture should detect the disconnect and save.
# Wait for it with a generous margin over the user timeout.
set +e
wait "${capture_pid}"
capture_exit=$?
set -e

if [[ "${capture_exit}" -ne 0 ]] && kill -0 "${capture_pid}" 2>/dev/null; then
    kill -TERM "${capture_pid}" 2>/dev/null || true
    sleep 1
    kill -KILL "${capture_pid}" 2>/dev/null || true
fi

if [[ "${projectv_exit}" -ne 0 ]]; then
    printf 'tracy-capture: FAIL — ProjectV exited with code %d\n' "${projectv_exit}" >&2
    printf 'tracy-capture: last 30 log lines:\n' >&2
    tail -n 30 /tmp/projectv-tracy-capture.log >&2 || true
    exit 5
fi

if [[ "${capture_exit}" -eq 0 ]] && [[ -s "${output_path}" ]]; then
    printf 'tracy-capture: PASS — wrote %s (%s bytes)\n' \
        "${output_path}" "$(stat -c%s "${output_path}")"
    exit 0
fi

printf 'tracy-capture: FAIL — tracy-capture exited with code %d and output is missing/empty\n' "${capture_exit}" >&2
printf 'tracy-capture: tracy-capture log:\n' >&2
tail -n 30 "${output_path}.log" >&2 || true
exit 4
