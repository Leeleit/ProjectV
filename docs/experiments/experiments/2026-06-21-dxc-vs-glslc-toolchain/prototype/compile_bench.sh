#!/usr/bin/env bash
# compile_bench.sh — Compile representative ProjectV shaders via glslc and DXC,
# measure compile time, output SPIR-V size, validation pass.
#
# Usage: bash compile_bench.sh [iters=N] [output_dir=results/]

set -u

ITERS=${1:-20}
OUT_DIR=${2:-results}
mkdir -p "${OUT_DIR}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="${SCRIPT_DIR}/tools/dxc"
GLSL_DIR="${SCRIPT_DIR}/shaders_glsl"
HLSL_DIR="${SCRIPT_DIR}/shaders_hlsl"

DXC="${TOOLS_DIR}/dxc"
DXC_LIB="${TOOLS_DIR}/lib"
GLSLC="$(command -v glslc)"
SPIRV_VAL="$(command -v spirv-val)"
SPIRV_DIS="$(command -v spirv-dis)"

export LD_LIBRARY_PATH="${DXC_LIB}:${LD_LIBRARY_PATH:-}"

if [[ ! -x "${DXC}" ]]; then
    echo "FATAL: DXC not found at ${DXC}" >&2
    exit 1
fi
if [[ -z "${GLSLC}" ]]; then
    echo "FATAL: glslc not in PATH" >&2
    exit 1
fi
if [[ -z "${SPIRV_VAL}" ]]; then
    echo "FATAL: spirv-val not in PATH" >&2
    exit 1
fi

echo "===== DXC version ====="
"${DXC}" --version 2>&1 | head -3
echo "===== glslc version ====="
"${GLSLC}" --version 2>&1 | head -2
echo "===== spirv-val version ====="
"${SPIRV_VAL}" --version 2>&1 | head -1
echo

# Common DXC flags for Vulkan 1.4 SPIR-V output.
# NOTE: DXC does NOT accept "-fspv-target-env=vulkan1.4" — the latest supported
# is "vulkan1.1spirv1.4" (SPIR-V 1.4 + Vulkan 1.1 base). SPIR-V 1.4 is sufficient
# for our Vulkan 1.4 features; we add -fspv-extension for newer extensions.
DXC_BASE_FLAGS=(
    -spirv
    -fspv-target-env=vulkan1.1spirv1.4
    -fvk-use-scalar-layout
)

# Common glslc flags for Vulkan 1.4 SPIR-V output.
#   --target-env=vulkan1.4
#   -fshader-stage=  (inferred from extension)
GLSLC_BASE_FLAGS=(
    --target-env=vulkan1.4
)

# Map of shaders: glsl_path|hlsl_path|stage|dxc_profile|extra_dxc_flags
SHADERS=(
    "voxel_minimal.vert|voxel_minimal.vert.hlsl|vertex|vs_6_0|"
    "voxel_minimal.frag|voxel_minimal.frag.hlsl|fragment|ps_6_0|"
    "voxel_mesh_minimal.mesh|voxel_mesh_minimal.mesh.hlsl|mesh|ms_6_5|-fspv-extension=SPV_EXT_mesh_shader"
    "hzb_cull_minimal.comp|hzb_cull_minimal.comp.hlsl|compute|cs_6_0|"
    "fluid_ca_minimal.comp|fluid_ca_minimal.comp.hlsl|compute|cs_6_0|"
)

# Output CSV headers.
CSV="${OUT_DIR}/compile_metrics.csv"
HEADER="shader,toolchain,iter,compile_ms,spv_size_bytes,validation_pass"
echo "${HEADER}" > "${CSV}"

# Stats helper (per-toolchain per-shader aggregate).
SUMMARY="${OUT_DIR}/compile_summary.csv"
HEADER_SUM="shader,toolchain,iters,mean_ms,median_ms,p95_ms,p99_ms,std_ms,mean_spv_bytes,validation_rate"
echo "${HEADER_SUM}" > "${SUMMARY}"

# Helper: median over an array.
median() {
    python3 -c "
import sys, statistics
xs = sorted(map(float, sys.stdin.read().split()))
n = len(xs)
if n == 0:
    print('nan'); sys.exit(0)
print(f'{statistics.median(xs):.3f}')
"
}

percentile() {
    local pct="$1"
    python3 -c "
import sys
xs = sorted(map(float, sys.stdin.read().split()))
n = len(xs)
if n == 0:
    print('nan'); sys.exit(0)
k = max(0, min(n - 1, int(round(float('${pct}') / 100.0 * (n - 1)))))
print(f'{xs[k]:.3f}')
"
}

stddev() {
    python3 -c "
import sys, statistics
xs = list(map(float, sys.stdin.read().split()))
if len(xs) < 2:
    print('0.000'); sys.exit(0)
print(f'{statistics.stdev(xs):.3f}')
"
}

mean() {
    python3 -c "
import sys
xs = list(map(float, sys.stdin.read().split()))
if not xs:
    print('nan'); sys.exit(0)
print(f'{sum(xs)/len(xs):.3f}')
"
}

# Helper: per-iter compile of one toolchain.
compile_once() {
    local toolchain="$1"
    local input="$2"
    local output="$3"
    shift 3
    local start_ns
    local end_ns
    local ms
    start_ns=$(date +%s%N)
    if [[ "${toolchain}" == "dxc" ]]; then
        # DXC uses -Fo <output>; glslc uses -o <output>.
        if ! "$@" -Fo "${output}" "${input}" 2>"${OUT_DIR}/.err"; then
            echo "COMPILE_FAIL"
            return 1
        fi
    else
        if ! "$@" "${input}" -o "${output}" 2>"${OUT_DIR}/.err"; then
            echo "COMPILE_FAIL"
            return 1
        fi
    fi
    end_ns=$(date +%s%N)
    ms=$(python3 -c "print(f'{(${end_ns} - ${start_ns}) / 1_000_000:.3f}')")
    echo "${ms}"
    return 0
}

# Run all.
for entry in "${SHADERS[@]}"; do
    IFS='|' read -r GLSL_NAME HLSL_NAME STAGE DXC_PROFILE DXC_EXTRA <<< "${entry}"
    GLSL_PATH="${GLSL_DIR}/${GLSL_NAME}"
    HLSL_PATH="${HLSL_DIR}/${HLSL_NAME}"

    echo "===== ${GLSL_NAME} / ${HLSL_NAME} (${STAGE}) ====="

    # glslc iter loop.
    for ((iter = 1; iter <= ITERS; iter++)); do
        OUT_GLSLC="${OUT_DIR}/${GLSL_NAME%.vert}.glslc.iter${iter}.spv"
        MS=$(compile_once glslc "${GLSL_PATH}" "${OUT_GLSLC}" "${GLSLC}" "${GLSLC_BASE_FLAGS[@]}")
        if [[ "${MS}" == "COMPILE_FAIL" ]]; then
            echo "${GLSL_NAME},glslc,${iter},NA,NA,0" >> "${CSV}"
            continue
        fi
        SIZE=$(stat -c%s "${OUT_GLSLC}" 2>/dev/null || echo 0)
        VALID=0
        if "${SPIRV_VAL}" --target-env vulkan1.4 "${OUT_GLSLC}" >/dev/null 2>&1; then
            VALID=1
        fi
        echo "${GLSL_NAME},glslc,${iter},${MS},${SIZE},${VALID}" >> "${CSV}"
    done

    # dxc iter loop.
    for ((iter = 1; iter <= ITERS; iter++)); do
        OUT_DXC="${OUT_DIR}/${HLSL_NAME%.hlsl}.dxc.iter${iter}.spv"
        MS=$(compile_once dxc "${HLSL_PATH}" "${OUT_DXC}" "${DXC}" -T "${DXC_PROFILE}" "${DXC_BASE_FLAGS[@]}" ${DXC_EXTRA})
        if [[ "${MS}" == "COMPILE_FAIL" ]]; then
            echo "${GLSL_NAME},dxc,${iter},NA,NA,0" >> "${CSV}"
            continue
        fi
        SIZE=$(stat -c%s "${OUT_DXC}" 2>/dev/null || echo 0)
        VALID=0
        if "${SPIRV_VAL}" --target-env vulkan1.4 "${OUT_DXC}" >/dev/null 2>&1; then
            VALID=1
        fi
        echo "${GLSL_NAME},dxc,${iter},${MS},${SIZE},${VALID}" >> "${CSV}"
    done

    # Aggregate per shader + toolchain.
    for toolchain in glslc dxc; do
        grep "^${GLSL_NAME},${toolchain}," "${CSV}" | awk -F, '$4 != "NA" {print $4}' > "${OUT_DIR}/.tmp_ms"
        grep "^${GLSL_NAME},${toolchain}," "${CSV}" | awk -F, '$5 != "NA" {print $5}' > "${OUT_DIR}/.tmp_size"
        grep "^${GLSL_NAME},${toolchain}," "${CSV}" | awk -F, '{sum+=$6} END {if (NR>0) print sum/NR; else print 0}' > "${OUT_DIR}/.tmp_valid"

        MEAN_MS=$(mean < "${OUT_DIR}/.tmp_ms")
        MEDIAN_MS=$(median < "${OUT_DIR}/.tmp_ms")
        P95_MS=$(percentile 95 < "${OUT_DIR}/.tmp_ms")
        P99_MS=$(percentile 99 < "${OUT_DIR}/.tmp_ms")
        STD_MS=$(stddev < "${OUT_DIR}/.tmp_ms")
        MEAN_SIZE=$(mean < "${OUT_DIR}/.tmp_size")
        VALID_RATE=$(cat "${OUT_DIR}/.tmp_valid")

        echo "${GLSL_NAME},${toolchain},${ITERS},${MEAN_MS},${MEDIAN_MS},${P95_MS},${P99_MS},${STD_MS},${MEAN_SIZE},${VALID_RATE}" >> "${SUMMARY}"
    done

    echo
done

echo "===== Summary ====="
column -s, -t "${SUMMARY}" 2>/dev/null || cat "${SUMMARY}"
echo
echo "Raw: ${CSV}"
echo "Summary: ${SUMMARY}"