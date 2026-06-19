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

