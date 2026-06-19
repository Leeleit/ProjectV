#include "frustum_cull.hpp"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct ProjectvCPlane3 {
    float n[3];
    float offset;
} ProjectvCPlane3;

static inline float projectv_absf(float value) {
    return value < 0.0f ? -value : value;
}

static inline float projectv_dot3(
    float ax, float ay, float az,
    float bx, float by, float bz) {
    return ax * bx + ay * by + az * bz;
}

static inline void projectv_build_planes(
    const ProjectvCFrustumCullParameters *parameters,
    ProjectvCPlane3 planes[5]) {
    const float fx = parameters->cameraForward[0];
    const float fy = parameters->cameraForward[1];
    const float fz = parameters->cameraForward[2];
    const float rx = parameters->cameraRight[0];
    const float ry = parameters->cameraRight[1];
    const float rz = parameters->cameraRight[2];
    const float ux = parameters->cameraUp[0];
    const float uy = parameters->cameraUp[1];
    const float uz = parameters->cameraUp[2];
    const float th = parameters->tanHalfHorizontalFov;
    const float tv = parameters->tanHalfVerticalFov;

    // **Plane 0: near** (offset = nearPlane; the
    // `passesPlane` test subtracts the offset, so the
    // cull test is `dot(toCenter, forward) +
    // projectedRadius < nearPlane`).
    planes[0].n[0] = fx;
    planes[0].n[1] = fy;
    planes[0].n[2] = fz;
    planes[0].offset = parameters->nearPlane;

    // **Planes 1..4: left/right/bottom/top** (offset = 0).
    planes[1].n[0] = fx * th + rx;
    planes[1].n[1] = fy * th + ry;
    planes[1].n[2] = fz * th + rz;
    planes[1].offset = 0.0f;

    planes[2].n[0] = fx * th - rx;
    planes[2].n[1] = fy * th - ry;
    planes[2].n[2] = fz * th - rz;
    planes[2].offset = 0.0f;

    planes[3].n[0] = fx * tv + ux;
    planes[3].n[1] = fy * tv + uy;
    planes[3].n[2] = fz * tv + uz;
    planes[3].offset = 0.0f;

    planes[4].n[0] = fx * tv - ux;
    planes[4].n[1] = fy * tv - uy;
    planes[4].n[2] = fz * tv - uz;
    planes[4].offset = 0.0f;
}

void projectv_cull_frustum_scalar(
    uint8_t *visible_mask,
    const ProjectvCAabb *aabbs,
    const ProjectvCFrustumCullParameters *parameters,
    size_t count) {
    ProjectvCPlane3 planes[5];
    projectv_build_planes(parameters, planes);

    const float maxDistance = parameters->maxDistance;
    const float px = parameters->cameraPosition[0];
    const float py = parameters->cameraPosition[1];
    const float pz = parameters->cameraPosition[2];

    for (size_t i = 0; i < count; ++i) {
        const ProjectvCAabb aabb = aabbs[i];
        const float cx = (aabb.min[0] + aabb.max[0]) * 0.5f;
        const float cy = (aabb.min[1] + aabb.max[1]) * 0.5f;
        const float cz = (aabb.min[2] + aabb.max[2]) * 0.5f;
        const float hx = (aabb.max[0] - aabb.min[0]) * 0.5f;
        const float hy = (aabb.max[1] - aabb.min[1]) * 0.5f;
        const float hz = (aabb.max[2] - aabb.min[2]) * 0.5f;
        const float tx = cx - px;
        const float ty = cy - py;
        const float tz = cz - pz;

        int visible = 1;
        for (int p = 0; p < 5; ++p) {
            const float nx = planes[p].n[0];
            const float ny = planes[p].n[1];
            const float nz = planes[p].n[2];
            const float centerDistance = projectv_dot3(tx, ty, tz, nx, ny, nz) - planes[p].offset;
            const float projectedRadius = projectv_absf(nx) * hx + projectv_absf(ny) * hy + projectv_absf(nz) * hz;
            if (centerDistance + projectedRadius < 0.0f) {
                visible = 0;
                break;
            }
        }

        if (visible && maxDistance > 0.0f) {
            const float lengthSq = tx * tx + ty * ty + tz * tz;
            const float aabbRadius = sqrtf(hx * hx + hy * hy + hz * hz);
            const float maxCenterDistance = maxDistance + aabbRadius;
            if (lengthSq > maxCenterDistance * maxCenterDistance) {
                visible = 0;
            }
        }

        if (visible) {
            visible_mask[i / 8] |= (uint8_t)(1u << (i % 8));
        }
    }
}

#if defined(__AVX2__)
#include <immintrin.h>

// **AVX2 8-way batched cull.** `__attribute__((target("avx2")))`
// forces the function to be compiled with AVX2 instructions
// available; the rest of the binary stays at the global
// baseline level. This sidesteps the Clang 22 whole-TU
// AVX2 + ASan regression (issue #194008 in `agent/memory.md`)
// and keeps the ABI per-function — the C++ caller doesn't
// need to know whether AVX2 is available at the call site.
__attribute__((target("avx2"))) void projectv_cull_frustum_avx2(
    uint8_t *visible_mask,
    const ProjectvCAabb *aabbs,
    const ProjectvCFrustumCullParameters *parameters,
    size_t count) {
    ProjectvCPlane3 planes[5];
    projectv_build_planes(parameters, planes);

    const float maxDistance = parameters->maxDistance;
    const float px = parameters->cameraPosition[0];
    const float py = parameters->cameraPosition[1];
    const float pz = parameters->cameraPosition[2];

    // Pre-broadcast plane-normal abs values as scalar
    // floats; we `_mm256_set1_ps` them per batch (the
    // set1 is hoisted out of the inner loop body).
    const float abs_n[5][3] = {
        {projectv_absf(planes[0].n[0]), projectv_absf(planes[0].n[1]), projectv_absf(planes[0].n[2])},
        {projectv_absf(planes[1].n[0]), projectv_absf(planes[1].n[1]), projectv_absf(planes[1].n[2])},
        {projectv_absf(planes[2].n[0]), projectv_absf(planes[2].n[1]), projectv_absf(planes[2].n[2])},
        {projectv_absf(planes[3].n[0]), projectv_absf(planes[3].n[1]), projectv_absf(planes[3].n[2])},
        {projectv_absf(planes[4].n[0]), projectv_absf(planes[4].n[1]), projectv_absf(planes[4].n[2])},
    };

    // Compute maxCenterDistance^2 (used by the maxDistance
    // branch). aabbRadius varies per AABB, so the per-AABB
    // value is computed inside the loop.
    const bool useMaxDistance = maxDistance > 0.0f;

    // **Pre-broadcast the camera-position offset.** The
    // `toCenter = center - cameraPos` subtraction is done
    // in SIMD; the cameraPos is broadcast once outside
    // the loop.
    const __m256 v_px = _mm256_set1_ps(px);
    const __m256 v_py = _mm256_set1_ps(py);
    const __m256 v_pz = _mm256_set1_ps(pz);

    const size_t fullBatches = count / 8;
    const size_t tailStart = fullBatches * 8;

    for (size_t batch = 0; batch < fullBatches; ++batch) {
        const ProjectvCAabb *lane = &aabbs[batch * 8];

        // **Gather 8 AABBs into 6 `__m256`s** (cx, cy, cz,
        // hx, hy, hz). The `ProjectvCAabb` struct is
        // 32 B (4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 floats) per
        // AABB, so adjacent AABBs are 32 B apart in
        // memory. There is no contiguous SIMD load
        // possible from the AoS layout. Two paths:
        //
        // 1. `_mm256_setr_ps` with the 8 axis values
        //    computed inline. The compiler folds the
        //    scalar arithmetic into the SIMD constants
        //    and emits 6 `vmovaps` from stack / registers
        //    with no intermediate buffer round-trip. This
        //    is the path the AVX2 kernel takes.
        //
        // 2. (Reference, not used) gather via
        //    `_mm256_i32gather_ps` with an 8-int index
        //    vector [0,32,64,...,224] — high latency on
        //    most CPUs, not worth it for 8 lanes.
        const float cx0 = (lane[0].min[0] + lane[0].max[0]) * 0.5f;
        const float cy0 = (lane[0].min[1] + lane[0].max[1]) * 0.5f;
        const float cz0 = (lane[0].min[2] + lane[0].max[2]) * 0.5f;
        const float hx0 = (lane[0].max[0] - lane[0].min[0]) * 0.5f;
        const float hy0 = (lane[0].max[1] - lane[0].min[1]) * 0.5f;
        const float hz0 = (lane[0].max[2] - lane[0].min[2]) * 0.5f;
        const float cx1 = (lane[1].min[0] + lane[1].max[0]) * 0.5f;
        const float cy1 = (lane[1].min[1] + lane[1].max[1]) * 0.5f;
        const float cz1 = (lane[1].min[2] + lane[1].max[2]) * 0.5f;
        const float hx1 = (lane[1].max[0] - lane[1].min[0]) * 0.5f;
        const float hy1 = (lane[1].max[1] - lane[1].min[1]) * 0.5f;
        const float hz1 = (lane[1].max[2] - lane[1].min[2]) * 0.5f;
        const float cx2 = (lane[2].min[0] + lane[2].max[0]) * 0.5f;
        const float cy2 = (lane[2].min[1] + lane[2].max[1]) * 0.5f;
        const float cz2 = (lane[2].min[2] + lane[2].max[2]) * 0.5f;
        const float hx2 = (lane[2].max[0] - lane[2].min[0]) * 0.5f;
        const float hy2 = (lane[2].max[1] - lane[2].min[1]) * 0.5f;
        const float hz2 = (lane[2].max[2] - lane[2].min[2]) * 0.5f;
        const float cx3 = (lane[3].min[0] + lane[3].max[0]) * 0.5f;
        const float cy3 = (lane[3].min[1] + lane[3].max[1]) * 0.5f;
        const float cz3 = (lane[3].min[2] + lane[3].max[2]) * 0.5f;
        const float hx3 = (lane[3].max[0] - lane[3].min[0]) * 0.5f;
        const float hy3 = (lane[3].max[1] - lane[3].min[1]) * 0.5f;
        const float hz3 = (lane[3].max[2] - lane[3].min[2]) * 0.5f;
        const float cx4 = (lane[4].min[0] + lane[4].max[0]) * 0.5f;
        const float cy4 = (lane[4].min[1] + lane[4].max[1]) * 0.5f;
        const float cz4 = (lane[4].min[2] + lane[4].max[2]) * 0.5f;
        const float hx4 = (lane[4].max[0] - lane[4].min[0]) * 0.5f;
        const float hy4 = (lane[4].max[1] - lane[4].min[1]) * 0.5f;
        const float hz4 = (lane[4].max[2] - lane[4].min[2]) * 0.5f;
        const float cx5 = (lane[5].min[0] + lane[5].max[0]) * 0.5f;
        const float cy5 = (lane[5].min[1] + lane[5].max[1]) * 0.5f;
        const float cz5 = (lane[5].min[2] + lane[5].max[2]) * 0.5f;
        const float hx5 = (lane[5].max[0] - lane[5].min[0]) * 0.5f;
        const float hy5 = (lane[5].max[1] - lane[5].min[1]) * 0.5f;
        const float hz5 = (lane[5].max[2] - lane[5].min[2]) * 0.5f;
        const float cx6 = (lane[6].min[0] + lane[6].max[0]) * 0.5f;
        const float cy6 = (lane[6].min[1] + lane[6].max[1]) * 0.5f;
        const float cz6 = (lane[6].min[2] + lane[6].max[2]) * 0.5f;
        const float hx6 = (lane[6].max[0] - lane[6].min[0]) * 0.5f;
        const float hy6 = (lane[6].max[1] - lane[6].min[1]) * 0.5f;
        const float hz6 = (lane[6].max[2] - lane[6].min[2]) * 0.5f;
        const float cx7 = (lane[7].min[0] + lane[7].max[0]) * 0.5f;
        const float cy7 = (lane[7].min[1] + lane[7].max[1]) * 0.5f;
        const float cz7 = (lane[7].min[2] + lane[7].max[2]) * 0.5f;
        const float hx7 = (lane[7].max[0] - lane[7].min[0]) * 0.5f;
        const float hy7 = (lane[7].max[1] - lane[7].min[1]) * 0.5f;
        const float hz7 = (lane[7].max[2] - lane[7].min[2]) * 0.5f;

        const __m256 v_cx = _mm256_setr_ps(cx0, cx1, cx2, cx3, cx4, cx5, cx6, cx7);
        const __m256 v_cy = _mm256_setr_ps(cy0, cy1, cy2, cy3, cy4, cy5, cy6, cy7);
        const __m256 v_cz = _mm256_setr_ps(cz0, cz1, cz2, cz3, cz4, cz5, cz6, cz7);
        const __m256 v_hx = _mm256_setr_ps(hx0, hx1, hx2, hx3, hx4, hx5, hx6, hx7);
        const __m256 v_hy = _mm256_setr_ps(hy0, hy1, hy2, hy3, hy4, hy5, hy6, hy7);
        const __m256 v_hz = _mm256_setr_ps(hz0, hz1, hz2, hz3, hz4, hz5, hz6, hz7);

        // **toCenter = center - cameraPos.** Broadcast
        // the camera position was hoisted out of the
        // loop. 3 independent subtractions.
        const __m256 v_tx = _mm256_sub_ps(v_cx, v_px);
        const __m256 v_ty = _mm256_sub_ps(v_cy, v_py);
        const __m256 v_tz = _mm256_sub_ps(v_cz, v_pz);

        // **5 plane culls.** For each plane: dot
        // (3 mul-add) + abs_mul_sum (3 mul-add) + compare
        // → 8-bit mask. We accumulate the cull bitmask
        // across the 5 planes (plus maxDistance below).
        uint32_t cullMask = 0;
        for (int p = 0; p < 5; ++p) {
            const __m256 v_nx = _mm256_set1_ps(planes[p].n[0]);
            const __m256 v_ny = _mm256_set1_ps(planes[p].n[1]);
            const __m256 v_nz = _mm256_set1_ps(planes[p].n[2]);
            const __m256 v_anx = _mm256_set1_ps(abs_n[p][0]);
            const __m256 v_any = _mm256_set1_ps(abs_n[p][1]);
            const __m256 v_anz = _mm256_set1_ps(abs_n[p][2]);
            const __m256 v_offset = _mm256_set1_ps(planes[p].offset);

            // **dot = tx*nx + ty*ny + tz*nz.** Three muls
            // + two adds. We then subtract the plane
            // offset to get `centerDistance`.
            const __m256 v_dot =
                _mm256_add_ps(
                    _mm256_add_ps(
                        _mm256_mul_ps(v_tx, v_nx),
                        _mm256_mul_ps(v_ty, v_ny)),
                    _mm256_mul_ps(v_tz, v_nz));
            const __m256 v_centerDistance = _mm256_sub_ps(v_dot, v_offset);

            // **projectedRadius = |nx|*hx + |ny|*hy + |nz|*hz.**
            // Identical arithmetic shape; no abs needed
            // (abs is folded into the pre-broadcast).
            const __m256 v_radius =
                _mm256_add_ps(
                    _mm256_add_ps(
                        _mm256_mul_ps(v_hx, v_anx),
                        _mm256_mul_ps(v_hy, v_any)),
                    _mm256_mul_ps(v_hz, v_anz));

            // **Cull test: centerDistance + radius < 0.**
            // `_mm256_cmp_ps` returns 0xFFFFFFFF for
            // "true" and 0 for "false" per lane. We
            // `_mm256_movemask_ps` to extract the 8 bits
            // (high bit of each lane's float) into a
            // uint8_t, then OR into the cumulative
            // cullMask.
            const __m256 v_sum = _mm256_add_ps(v_centerDistance, v_radius);
            const __m256 v_culled = _mm256_cmp_ps(v_sum, _mm256_setzero_ps(), _CMP_LT_OQ);
            const uint32_t planeMask = (uint32_t)_mm256_movemask_ps(v_culled);
            cullMask |= planeMask;
        }

        // **maxDistance test.** Per-AABB: lengthSq
        // (toCenter) > (maxDistance + aabbRadius)^2. We
        // compute lengthSq in SIMD; aabbRadius is per-AABB
        // (different half_extents) so we already have it
        // in v_hx/v_hy/v_hz — sqrtf lane-wise and
        // compare. The branch is taken only if
        // useMaxDistance is true (set once outside the
        // batch loop).
        if (useMaxDistance) {
            const __m256 v_lengthSq =
                _mm256_add_ps(
                    _mm256_add_ps(
                        _mm256_mul_ps(v_tx, v_tx),
                        _mm256_mul_ps(v_ty, v_ty)),
                    _mm256_mul_ps(v_tz, v_tz));
            // aabbRadius = sqrt(hx*hx + hy*hy + hz*hz)
            // per lane. We do the sqrts on the
            // already-computed half-extent SIMD lanes.
            const __m256 v_heSq =
                _mm256_add_ps(
                    _mm256_add_ps(
                        _mm256_mul_ps(v_hx, v_hx),
                        _mm256_mul_ps(v_hy, v_hy)),
                    _mm256_mul_ps(v_hz, v_hz));
            const __m256 v_aabbRadius = _mm256_sqrt_ps(v_heSq);
            const __m256 v_maxCenter = _mm256_set1_ps(maxDistance);
            const __m256 v_maxCenterDistance = _mm256_add_ps(v_maxCenter, v_aabbRadius);
            const __m256 v_maxCenterDistanceSq = _mm256_mul_ps(v_maxCenterDistance, v_maxCenterDistance);
            const __m256 v_tooFar = _mm256_cmp_ps(v_lengthSq, v_maxCenterDistanceSq, _CMP_GT_OQ);
            const uint32_t farMask = (uint32_t)_mm256_movemask_ps(v_tooFar);
            cullMask |= farMask;
        }

        // **Pack the 8 visible bits into the caller's
        // mask byte.** `cullMask` is the bitwise-OR of
        // all plane-cull masks; AABB i is visible iff
        // bit i is NOT set.
        const uint8_t visible = (uint8_t)(~cullMask & 0xFFu);
        visible_mask[batch] |= visible;
    }

    // **Tail (count % 8 != 0).** The scalar kernel
    // handles 1..7 remaining AABBs. We share the
    // already-built planes struct, so the math is
    // identical to the scalar path.
    for (size_t i = tailStart; i < count; ++i) {
        const ProjectvCAabb aabb = aabbs[i];
        const float cx = (aabb.min[0] + aabb.max[0]) * 0.5f;
        const float cy = (aabb.min[1] + aabb.max[1]) * 0.5f;
        const float cz = (aabb.min[2] + aabb.max[2]) * 0.5f;
        const float hx = (aabb.max[0] - aabb.min[0]) * 0.5f;
        const float hy = (aabb.max[1] - aabb.min[1]) * 0.5f;
        const float hz = (aabb.max[2] - aabb.min[2]) * 0.5f;
        const float tx = cx - px;
        const float ty = cy - py;
        const float tz = cz - pz;

        int visible = 1;
        for (int p = 0; p < 5; ++p) {
            const float nx = planes[p].n[0];
            const float ny = planes[p].n[1];
            const float nz = planes[p].n[2];
            const float centerDistance = projectv_dot3(tx, ty, tz, nx, ny, nz) - planes[p].offset;
            const float projectedRadius = projectv_absf(nx) * hx + projectv_absf(ny) * hy + projectv_absf(nz) * hz;
            if (centerDistance + projectedRadius < 0.0f) {
                visible = 0;
                break;
            }
        }
        if (visible && useMaxDistance) {
            const float lengthSq = tx * tx + ty * ty + tz * tz;
            const float aabbRadius = sqrtf(hx * hx + hy * hy + hz * hz);
            const float maxCenterDistance = maxDistance + aabbRadius;
            if (lengthSq > maxCenterDistance * maxCenterDistance) {
                visible = 0;
            }
        }
        if (visible) {
            visible_mask[i / 8] |= (uint8_t)(1u << (i % 8));
        }
    }
}
#endif
