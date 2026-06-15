// **Tier 3 (`2026-06-13`).** C / intrinsics
// `frustum_cull` kernel. The C ABI / `extern "C"` boundary
// lets the C++ engine call a hand-rolled AVX2 kernel with
// no name-mangling or overload-resolution cost, and lets
// Godbolt review the intrinsics 1:1 against the compiler's
// autovectorized output (see `bench/FrustumCullBenchmark.cpp`).
//
// The 6-plane frustum cull here matches the algorithm in
// `src/render/SceneResources.hpp::IsAabbVisibleAgainstCameraFrustum`
// (the M5 helper). Same math, same byte layout, but laid
// out so 8 AABBs share the same 6 precomputed plane
// normals — the per-plane inner loop is 8 dots + 8 abs-mul
// + 8 sums per call instead of 1 dot + 1 abs-mul + 1 sum
// per AABB.
//
// **API contract.**
// - `visible_mask[i / 8] & (1u << (i % 8))` is set iff the
//   AABB is visible. Bits for indices `>= count` are
//   preserved (caller-owned, we do not zero unused lanes).
// - `count` may be any non-zero value. Tail lanes (count
//   not divisible by 8) are still computed; caller masks
//   them off. This keeps the inner loop branch-free.
// - `parameters` is the same struct the C++ helper takes
//   (camera position / forward / right / up / fovs /
//   near / max-distance), but in plain C float[3] form
//   for ABI safety — `__attribute__((target("avx2")))`
//   is per-function and never crosses a translation
//   unit boundary, so the parameters stay scalar.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProjectvCFrustumCullParameters {
    float cameraPosition[3];
    float maxDistance;
    float cameraForward[3];
    float tanHalfVerticalFov;
    float cameraRight[3];
    float tanHalfHorizontalFov;
    float cameraUp[3];
    float nearPlane;
} ProjectvCFrustumCullParameters;

typedef struct ProjectvCAabb {
    float min[3];
    float _pad0;
    float max[3];
    float _pad1;
} ProjectvCAabb;

void projectv_cull_frustum_scalar(
    uint8_t *visible_mask,
    const ProjectvCAabb *aabbs,
    const ProjectvCFrustumCullParameters *parameters,
    size_t count);

#if defined(__AVX2__)
void projectv_cull_frustum_avx2(
    uint8_t *visible_mask,
    const ProjectvCAabb *aabbs,
    const ProjectvCFrustumCullParameters *parameters,
    size_t count);
#endif

#ifdef __cplusplus
}
#endif

