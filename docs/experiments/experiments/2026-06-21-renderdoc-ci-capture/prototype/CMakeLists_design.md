# CMakeLists_design.md — production integration proposal

> **Status:** design proposal. NOT yet integrated into mainline `CMakeLists.txt`.
> **Target:** `docs/experiments/AGENTS.md §6 Definition of done` integration recommendation.

## ProjectV mainline CMakeLists.txt integration

Per `agent/knowledge.md` 3-step migration precedent + `agent/knowledge.md` build/verification
contract, добавить:

```cmake
# === docs/experiments/2026-06-21-renderdoc-ci-capture integration (Step 1) ===

option(PROJECTV_CI_PIXEL_DIFF
    "Enable RenderDoc capture regression CI (tests/regression/golden/)"
    OFF)

if(PROJECTV_CI_PIXEL_DIFF)
    message(STATUS "ProjectV CI: RenderDoc pixel-diff regression enabled")
    message(STATUS "  Golden images dir: tests/regression/golden/")
    message(STATUS "  Capture script: scripts/ci_capture.sh")
    message(STATUS "  Install: pacman -S renderdoc (Arch) | apt install renderdoc (Ubuntu)")

    # Capture trigger env integration
    if(DEFINED ENV{PROJECTV_CAPTURE_TRIGGER})
        message(STATUS "  PROJECTV_CAPTURE_TRIGGER=$ENV{PROJECTV_CAPTURE_TRIGGER}")
        add_definitions(-DPROJECTV_CAPTURE_TRIGGER="$ENV{PROJECTV_CAPTURE_TRIGGER}")
    endif()

    # Capture range env (Step 2 E_SelectiveCaptureRange)
    if(DEFINED ENV{PROJECTV_CAPTURE_START_FRAME})
        message(STATUS "  PROJECTV_CAPTURE_START_FRAME=$ENV{PROJECTV_CAPTURE_START_FRAME}")
    endif()

    # Register regression test subdirectory
    add_subdirectory(tests/regression)
endif()

# === tests/regression/CMakeLists.txt ===

if(PROJECTV_CI_PIXEL_DIFF)
    # ImageMagick `compare` for PSNR + SSIM (cross-platform CLI fallback)
    find_program(IMAGEMAGICK_COMPARE compare
        DOC "ImageMagick compare tool for PSNR/SSIM")

    if(NOT IMAGEMAGICK_COMPARE)
        message(WARNING "PROJECTV_CI_PIXEL_DIFF=ON but ImageMagick compare not found")
    endif()

    add_executable(image_diff
        image_diff.cpp   # C++ helper — PSNR per Akenine-Möller + SSIM per Wang 2004
    )
    target_link_libraries(image_diff PRIVATE stb::stb_image)

    # Register CTest
    add_test(NAME ProjectVRegressionCaptureTests
        COMMAND ${CMAKE_COMMAND}
            -DGOLDEN_DIR=${CMAKE_SOURCE_DIR}/tests/regression/golden
            -DCAPTURE_SCRIPT=${CMAKE_SOURCE_DIR}/scripts/ci_capture.sh
            -DBINARY=$<TARGET_FILE:ProjectV>
            -DIMAGE_DIFF=$<TARGET_FILE:image_diff>
            -P ${CMAKE_SOURCE_DIR}/tests/regression/run_capture_tests.cmake)
    set_tests_properties(ProjectVRegressionCaptureTests PROPERTIES
        LABELS "ci;regression;renderdoc")
endif()
```

## scripts/ci_capture.sh — headless capture wrapper

Per `sources.md §5-7` (`renderdoccmd` + `rdc-cli` patterns):

```bash
#!/bin/bash
# ci_capture.sh — headless RenderDoc capture wrapper для CI regression tests.
# Usage: PROJECTV_CAPTURE_TRIGGER=baseline ./scripts/ci_capture.sh tests/regression/golden/depth_prepass_frame1.rdc
set -euo pipefail

CAPTURE_FILE="${1:-capture.rdc}"
TRIGGER="${PROJECTV_CAPTURE_TRIGGER:-baseline}"
START_FRAME="${PROJECTV_CAPTURE_START_FRAME:-0}"
NUM_FRAMES="${PROJECTV_CAPTURE_NUM_FRAMES:-1}"

# Verify RenderDoc is installed
if ! command -v renderdoccmd &>/dev/null; then
    echo "ERROR: renderdoccmd not found. Install: pacman -S renderdoc (Arch) | apt install renderdoc (Ubuntu)"
    exit 1
fi

# Verify Vulkan layer is registered (Linux)
if [ ! -f "/usr/share/vulkan/implicit_layer.d/VkLayer_RenderDoc_capture.json" ] \
    && [ ! -f "/etc/vulkan/implicit_layer.d/VkLayer_RenderDoc_capture.json" ] \
    && [ ! -f "$HOME/.local/share/vulkan/implicit_layer.d/VkLayer_RenderDoc_capture.json" ]; then
    echo "ERROR: RenderDoc Vulkan layer not registered. Run: renderdoccmd register"
    exit 1
fi

# Set RenderDoc env
export RENDERDOC_CAP_CAPTURE_CALLSTACKS=0
export RENDERDOC_CAP_CAPTURE_ALL_CMD_LISTS=1
export RENDERDOC_CAP_REF_ALL_RESOURCES=1
export RENDERDOC_OVERRIDE_VULKAN_LAYER=VK_LAYER_RENDERDOC_capture

# Headless capture — auto-trigger after START_FRAME, NUM_FRAMES captures
echo "Capturing $NUM_FRAMES frame(s) to $CAPTURE_FILE (trigger=$TRIGGER, start=$START_FRAME)"
renderdoccmd capture \
    --capture-file "$CAPTURE_FILE" \
    --num-frames "$NUM_FRAMES" \
    --frame-start "$START_FRAME" \
    --exe ./build/linux-clang-debug/ProjectV \
    --working-dir . \
    -- --projectv-ci-pixel-diff

# Replay + extract image for imageDiff
echo "Replaying capture and extracting final color attachment..."
renderdoccmd replay --capture "$CAPTURE_FILE" --export final_color.png
echo "Capture complete: $CAPTURE_FILE + final_color.png"
```

## tests/regression/image_diff.cpp — PSNR + SSIM helper

Per `sources.md §18-21` (PSNR/SSIM formulas per Akenine-Möller / Wang 2004):

```cpp
// SPDX-License-Identifier: MIT
//
// image_diff.cpp — PSNR + SSIM image diff helper для RenderDoc golden regression tests.
// Usage: image_diff golden.png candidate.png [psnr_threshold_db] [ssim_threshold]

#include <cmath>
#include <cstdio>
#include <vector>

// stb_image single-header library (vendored в external/stb/ per ProjectV build matrix)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static double psnr(const uint8_t* a, const uint8_t* b, int n) {
    double mse = 0.0;
    for (int i = 0; i < n; ++i) {
        const double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        mse += diff * diff;
    }
    mse /= static_cast<double>(n);
    if (mse < 1e-10) return std::numeric_limits<double>::infinity();
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

static double ssim_luminance(double mu_a, double mu_b, double sigma_a, double sigma_b,
                              double sigma_ab) {
    constexpr double C1 = 6.5025;   // (0.01 * 255)^2
    constexpr double C2 = 58.5225;  // (0.03 * 255)^2
    const double num = (2.0 * mu_a * mu_b + C1) * (2.0 * sigma_ab + C2);
    const double den = (mu_a * mu_a + mu_b * mu_b + C1) * (sigma_a * sigma_a + sigma_b * sigma_b + C2);
    return num / den;
}

// Simplified SSIM per Wang 2004 (global, not block-based — adequate for regression detection).
static double ssim(const uint8_t* a, const uint8_t* b, int n) {
    double sum_a = 0.0, sum_b = 0.0;
    for (int i = 0; i < n; ++i) { sum_a += a[i]; sum_b += b[i]; }
    const double mu_a = sum_a / n;
    const double mu_b = sum_b / n;
    double var_a = 0.0, var_b = 0.0, cov_ab = 0.0;
    for (int i = 0; i < n; ++i) {
        const double da = a[i] - mu_a;
        const double db = b[i] - mu_b;
        var_a += da * da;
        var_b += db * db;
        cov_ab += da * db;
    }
    const double sigma_a = std::sqrt(var_a / n);
    const double sigma_b = std::sqrt(var_b / n);
    const double sigma_ab = cov_ab / n;
    return ssim_luminance(mu_a, mu_b, sigma_a, sigma_b, sigma_ab);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s golden.png candidate.png [psnr_threshold_db=50.0] [ssim_threshold=0.995]\n", argv[0]);
        return 2;
    }
    const char* golden_path = argv[1];
    const char* candidate_path = argv[2];
    const double psnr_threshold = argc > 3 ? std::atof(argv[3]) : 50.0;
    const double ssim_threshold = argc > 4 ? std::atof(argv[4]) : 0.995;

    int wa, ha, wb, hb, channels_a, channels_b;
    uint8_t* a = stbi_load(golden_path, &wa, &ha, &channels_a, 4);
    uint8_t* b = stbi_load(candidate_path, &wb, &hb, &channels_b, 4);
    if (!a || !b) { std::fprintf(stderr, "Failed to load images\n"); return 2; }
    if (wa != wb || ha != hb) {
        std::fprintf(stderr, "Image size mismatch: %dx%d vs %dx%d\n", wa, ha, wb, hb);
        return 2;
    }
    const int n = wa * ha * 4;
    const double psnr_db = psnr(a, b, n);
    const double ssim_val = ssim(a, b, n);
    stbi_image_free(a);
    stbi_image_free(b);

    std::printf("PSNR: %.2f dB (threshold %.2f dB) %s\n",
                psnr_db, psnr_threshold, psnr_db >= psnr_threshold ? "PASS" : "FAIL");
    std::printf("SSIM: %.4f (threshold %.4f) %s\n",
                ssim_val, ssim_threshold, ssim_val >= ssim_threshold ? "PASS" : "FAIL");

    return (psnr_db >= psnr_threshold && ssim_val >= ssim_threshold) ? 0 : 1;
}
```

## tests/regression/run_capture_tests.cmake — CTest driver

Per `sources.md §15` vision-regression-kit + Glint3D pattern:

```cmake
# Per-pass golden image regression tests.
# Per `docs/experiments/AGENTS.md §6 DoD` + `benchmarks/methodology.md §3`.

set(GOLDEN_DIR "${GOLDEN_DIR}")
set(CAPTURE_SCRIPT "${CAPTURE_SCRIPT}")
set(BINARY "${BINARY}")
set(IMAGE_DIFF "${IMAGE_DIFF}")

set(PROJECTV_PASSES
    depth_prepass
    hzb_cull
    hiz_mip_chain
    voxel_mesh
    csm_shadow
    opaque_forward
    vct_cone_march
    rtx_ray_query
    fluid_ca_pingpong
    taa_resolve
    transparent_fwd
    ui_debug
)

set(ENV{PROJECTV_CAPTURE_TRIGGER} "regression")
set(ENV{PROJECTV_CAPTURE_START_FRAME} "60")

foreach(pass ${PROJECTV_PASSES})
    set(capture_file "${CMAKE_BINARY_DIR}/regression/${pass}_capture.rdc")
    set(candidate_png "${CMAKE_BINARY_DIR}/regression/${pass}_candidate.png")
    set(golden_png "${GOLDEN_DIR}/${pass}_golden.png")

    add_test(NAME "ProjectVRegression_${pass}"
        COMMAND ${CMAKE_COMMAND}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DCAPTURE_FILE=${capture_file}
            -DCANDIDATE_PNG=${candidate_png}
            -DBINARY=${BINARY}
            -DGOLDEN_PNG=${golden_png}
            -DIMAGE_DIFF=${IMAGE_DIFF}
            -PPassRegressionTest.cmake)
    set_tests_properties("ProjectVRegression_${pass}" PROPERTIES
        LABELS "ci;regression;renderdoc;${pass}"
        TIMEOUT 120)
endforeach()
```

## tests/regression/PassRegressionTest.cmake — per-pass capture + diff

```cmake
execute_process(
    COMMAND bash ${CAPTURE_SCRIPT} ${CAPTURE_FILE}
    RESULT_VARIABLE capture_result)
if(NOT capture_result EQUAL 0)
    message(FATAL_ERROR "Capture failed for ${CAPTURE_FILE}")
endif()

execute_process(
    COMMAND ${IMAGE_DIFF} ${GOLDEN_PNG} ${CANDIDATE_PNG} 50.0 0.995
    RESULT_VARIABLE diff_result)

if(NOT diff_result EQUAL 0)
    message(FATAL_ERROR "Regression detected: ${CANDIDATE_PNG} differs from ${GOLDEN_PNG}")
endif()
```

## Effort estimate

- **Step 1 (XS, ~50 LoC):** CMakeLists + scripts/ci_capture.sh — 1 session.
- **Step 2 (M, ~250 LoC):** image_diff.cpp + tests/regression/ + 12 initial golden captures
  (manual capture run + commit) — 1-2 sessions.
- **Step 3 (S, ~100 LoC):** .github/workflows/capture.yml + Slack webhook — 1 session.

**Total ~400 LoC, S-M effort, 2-3 sessions** — matches `RESULTS.md §6` + `README.md §7` recommendations.

## Dependencies

- **RenderDoc 1.44+** (Vulkan 1.4 support, captures via Vulkan layer)
  - Arch Linux: `pacman -S renderdoc`
  - Ubuntu: `apt install renderdoc` (snap alternative)
  - macOS: `brew install renderdoc`
  - Windows: official MSI from renderdoc.org
- **ImageMagick** (optional fallback for `compare -metric PSNR`)
- **stb_image** (single-header image loader — vendored в `external/stb/` per ProjectV conventions)

## Cross-references

- `agent/knowledge.md` — `PROJECTV_ENABLE_RENDERDOC_MARKERS` (existing integration)
- `agent/knowledge.md` — build/verification contract
- `agent/knowledge.md` — 3-step migration precedent
- `docs/experiments/2026-06-21-renderdoc-ci-capture/sources.md` — primary references
- `docs/experiments/2026-06-21-renderdoc-ci-capture/RESULTS.md` — measured results
