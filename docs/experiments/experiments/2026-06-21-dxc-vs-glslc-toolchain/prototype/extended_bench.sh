#!/usr/bin/env bash
# extended_bench.sh — Extended measurement: debug info overhead + SPIR-V
# instruction count + optimization level impact.
#
# Usage: bash extended_bench.sh

set -u

ITERS=${1:-30}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/results/extended"
mkdir -p "${OUT_DIR}"

DXC="${SCRIPT_DIR}/tools/dxc/dxc"
DXC_LIB="${SCRIPT_DIR}/tools/dxc/lib"
GLSLC="$(command -v glslc)"
SPIRV_VAL="$(command -v spirv-val)"
SPIRV_DIS="$(command -v spirv-dis)"
export LD_LIBRARY_PATH="${DXC_LIB}:${LD_LIBRARY_PATH:-}"

GLSL_DIR="${SCRIPT_DIR}/shaders_glsl"
HLSL_DIR="${SCRIPT_DIR}/shaders_hlsl"

CSV="${OUT_DIR}/extended_metrics.csv"
echo "shader,mode,toolchain,iter,compile_ms,spv_size_bytes,validation_pass,spirv_inst_count" > "${CSV}"

# Helper functions (same as compile_bench.sh).
median() { python3 -c "import sys, statistics; xs=sorted(map(float, sys.stdin.read().split())); print(f'{statistics.median(xs):.3f}' if xs else 'nan')"; }
percentile() { python3 -c "import sys; xs=sorted(map(float, sys.stdin.read().split())); n=len(xs); k=max(0, min(n-1, int(round(float('$1')/100.0*(n-1))))); print(f'{xs[k]:.3f}' if n else 'nan')"; }
mean() { python3 -c "import sys; xs=list(map(float, sys.stdin.read().split())); print(f'{sum(xs)/len(xs):.3f}' if xs else 'nan')"; }

# Modes to test:
#   "default"    — no debug, default optimization
#   "debug"      — with debug info (DXC: -Zi; glslc: -g)
#   "optimize"   — with optimization (DXC: -O3; glslc: no -O flag for default)
MODES=(
    "default|-O0"
    "debug|-Zi"  # glslc gets -g instead
    "optimize|-O3"
)

# Shader configs.
SHADERS=(
    "voxel_minimal.vert|vert|vs_6_0|"
    "voxel_minimal.frag|frag|ps_6_0|"
    "voxel_mesh_minimal.mesh|mesh|ms_6_5|-fspv-extension=SPV_EXT_mesh_shader"
    "hzb_cull_minimal.comp|comp|cs_6_0|"
    "fluid_ca_minimal.comp|comp|cs_6_0|"
)

# Helper: compile once with custom flags.
compile_with_flags() {
    local toolchain="$1"
    local input="$2"
    local output="$3"
    shift 3
    local start_ns
    start_ns=$(date +%s%N)
    if [[ "${toolchain}" == "dxc" ]]; then
        if ! "$@" -Fo "${output}" "${input}" 2>"${OUT_DIR}/.err"; then
            echo "FAIL"
            return 1
        fi
    else
        if ! "$@" "${input}" -o "${output}" 2>"${OUT_DIR}/.err"; then
            echo "FAIL"
            return 1
        fi
    fi
    local end_ns
    end_ns=$(date +%s%N)
    python3 -c "print(f'{(${end_ns} - ${start_ns}) / 1_000_000:.3f}')"
}

count_spirv_instructions() {
    local spv="$1"
    if [[ ! -f "${spv}" ]]; then
        echo "0"
        return
    fi
    "${SPIRV_DIS}" --raw-id "${spv}" 2>/dev/null | wc -l
}

for entry in "${SHADERS[@]}"; do
    IFS='|' read -r BASE_NAME STAGE DXC_PROFILE DXC_EXTRA <<< "${entry}"
    GLSL_PATH="${GLSL_DIR}/${BASE_NAME}"
    HLSL_PATH="${HLSL_DIR}/${BASE_NAME}.hlsl"

    for mode_entry in "${MODES[@]}"; do
        IFS='|' read -r MODE_NAME MODE_FLAGS <<< "${mode_entry}"

        echo "===== ${BASE_NAME} / ${MODE_NAME} ====="

        # glslc mode.
        GLSLC_FLAGS=(--target-env=vulkan1.4)
        case "${MODE_NAME}" in
            default) ;;
            debug) GLSLC_FLAGS+=(-g) ;;
            optimize) ;;  # glslc default is already O (no explicit flag)
        esac

        for ((iter = 1; iter <= ITERS; iter++)); do
            OUT_GLSLC="${OUT_DIR}/${BASE_NAME}.glslc.${MODE_NAME}.iter${iter}.spv"
            MS=$(compile_with_flags glslc "${GLSL_PATH}" "${OUT_GLSLC}" "${GLSLC}" "${GLSLC_FLAGS[@]}")
            if [[ "${MS}" == "FAIL" ]]; then
                echo "${BASE_NAME},${MODE_NAME},glslc,${iter},NA,NA,0,0" >> "${CSV}"
                continue
            fi
            SIZE=$(stat -c%s "${OUT_GLSLC}" 2>/dev/null || echo 0)
            VALID=0
            "${SPIRV_VAL}" --target-env vulkan1.4 "${OUT_GLSLC}" >/dev/null 2>&1 && VALID=1
            INST_COUNT=$(count_spirv_instructions "${OUT_GLSLC}")
            echo "${BASE_NAME},${MODE_NAME},glslc,${iter},${MS},${SIZE},${VALID},${INST_COUNT}" >> "${CSV}"
        done

        # dxc mode.
        DXC_FLAGS=(-spirv -fspv-target-env=vulkan1.1spirv1.4 -fvk-use-scalar-layout -T "${DXC_PROFILE}" ${DXC_EXTRA})
        case "${MODE_NAME}" in
            default) ;;
            debug) DXC_FLAGS+=(-Zi) ;;
            optimize) DXC_FLAGS+=(-O3) ;;
        esac

        for ((iter = 1; iter <= ITERS; iter++)); do
            OUT_DXC="${OUT_DIR}/${BASE_NAME}.dxc.${MODE_NAME}.iter${iter}.spv"
            MS=$(compile_with_flags dxc "${HLSL_PATH}" "${OUT_DXC}" "${DXC}" "${DXC_FLAGS[@]}")
            if [[ "${MS}" == "FAIL" ]]; then
                echo "${BASE_NAME},${MODE_NAME},dxc,${iter},NA,NA,0,0" >> "${CSV}"
                continue
            fi
            SIZE=$(stat -c%s "${OUT_DXC}" 2>/dev/null || echo 0)
            VALID=0
            "${SPIRV_VAL}" --target-env vulkan1.4 "${OUT_DXC}" >/dev/null 2>&1 && VALID=1
            INST_COUNT=$(count_spirv_instructions "${OUT_DXC}")
            echo "${BASE_NAME},${MODE_NAME},dxc,${iter},${MS},${SIZE},${VALID},${INST_COUNT}" >> "${CSV}"
        done

        # Aggregate.
        for toolchain in glslc dxc; do
            grep "^${BASE_NAME},${MODE_NAME},${toolchain}," "${CSV}" | awk -F, '$5 != "NA" {print $5}' > "${OUT_DIR}/.tmp_ms"
            grep "^${BASE_NAME},${MODE_NAME},${toolchain}," "${CSV}" | awk -F, '$6 != "NA" {print $6}' > "${OUT_DIR}/.tmp_size"
            grep "^${BASE_NAME},${MODE_NAME},${toolchain}," "${CSV}" | awk -F, '$7 == "1"' | wc -l > "${OUT_DIR}/.tmp_vcount"
            grep "^${BASE_NAME},${MODE_NAME},${toolchain}," "${CSV}" | awk -F, '$8 != "0" {print $8}' > "${OUT_DIR}/.tmp_inst"

            MEAN_MS=$(mean < "${OUT_DIR}/.tmp_ms")
            MEAN_SIZE=$(mean < "${OUT_DIR}/.tmp_size")
            MEAN_INST=$(mean < "${OUT_DIR}/.tmp_inst")
            VCOUNT=$(cat "${OUT_DIR}/.tmp_vcount")

            echo "  [${BASE_NAME} ${MODE_NAME} ${toolchain}] mean_ms=${MEAN_MS} size=${MEAN_SIZE} inst=${MEAN_INST} valid=${VCOUNT}/${ITERS}"
        done
    done
done

echo
echo "Extended metrics: ${CSV}"