#version 460

struct MaterialVisual {
    vec4 baseColor;
    vec4 surface;
    vec4 medium;
    vec4 shading;
};

struct ChunkDescriptor {
    ivec4 chunkOrigin;
    uvec4 chunkExtentAndNonAir;
    uvec4 voxelDataInfo;
    uvec4 drawRanges;
};

layout(set = 0, binding = 1, std430) readonly buffer PackedChunkDescriptors {
    ChunkDescriptor chunkDescriptors[];
};

layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    MaterialVisual materials[];
};

layout(set = 0, binding = 3, std430) readonly buffer SceneLightingBuffer {
    vec4 skyColorAndFogDensity;
    vec4 horizonColorAndFogStart;
    vec4 groundColorAndFogMax;
    vec4 sunColorAndIntensity;
    vec4 sunDirectionAndWrap;
    vec4 postProcess;
    vec4 sunShadowParams;
    vec4 sunContactShadowParams;
    vec4 ambientOcclusionParams;
    mat4 sunShadowViewProjections[4];
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 shadowCascadeDepthSplits;
    vec4 shadowCascadeBlendParams;
    vec4 localPointLightPositionAndRadius;
    vec4 localPointLightColorAndIntensity;
    vec4 localPointLightParams;
    // TAA contract (mirrors VoxelSceneLighting, see agent/decisions.md §18):
    // taaParams = (jitterX, jitterY, blend, enabled); prevViewProjectionMatrix
    // is the previous frame's viewProjection (used by the TAA resolve pass for
    // depth-based reprojection); taaHistoryParams = (texelX, texelY, valid, neighbourhoodRadius).
    // The voxel pass doesn't read .w, but the field is declared here so the
    // std430 layout matches `VoxelSceneLighting` byte-for-byte.
    // 1.5 anti-flicker layer history params (texelX, texelY, valid, blendFactor);
    // the voxel pass USES these for per-layer temporal blending of CTSH/AOCC/LOCL.
    vec4 taaParams;
    mat4 prevViewProjectionMatrix;
    vec4 taaHistoryParams;
    vec4 taaLayerHistoryParams;
} sceneLighting;

layout(set = 0, binding = 4) uniform sampler2DArrayShadow sunShadowMap;

layout(set = 0, binding = 5, std430) readonly buffer PackedChunkVoxelPayload {
    uint chunkVoxelWords[];
};

// 1.5 anti-flicker: per-layer temporal history. Voxel pass reads
// this at the current fragment UV, blends the freshly-computed
// raw CTSH/AOCC/LOCL with the previous frame's values, and uses
// the blended values for this frame's lighting. The freshly
// computed raw values are then written to the layer scene color
// target via the new `outLayerMask` MRT output (Location 2); a
// per-frame `vkCmdCopyImage` in `Renderer.cpp` makes the current
// frame's data the next frame's history input.
layout(set = 0, binding = 6) uniform sampler2D layerHistory;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 cameraForward;
    ivec4 worldMinAndChunkSize;
    uvec4 chunkGridAndFlags;
} pushConstants;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) flat in uint inMaterialIndex;
layout(location = 3) in float inAmbientVisibility;

#ifdef TAA_ENABLED
layout(location = 1) out vec4 outSceneColor;
#define OUT_COLOR outSceneColor
#else
layout(location = 0) out vec4 outColor;
#define OUT_COLOR outColor
#endif

// 1.5 anti-flicker: per-layer temporal mask. R = CTSH (sun contact
// shadow), G = AOCC (local ambient occlusion visibility), B = LOCL
// (local-point-light shadow visibility), A = 1.0 (reserved).
// Both TAA-on and TAA-off variants declare the same output so the
// pipeline's `pColorAttachmentFormats[2]` declaration (3 attachments
// total) stays valid for both. In TAA-on path the voxel pass uses
// 8 components (outSceneColor + outLayerMask); in TAA-off path
// 8 components (outColor + outLayerMask). The third attachment
// slot is bound in both paths but the TAA-on variant doesn't write
// to slot 0 (outColor) and the TAA-off variant doesn't write to
// slot 1 (outSceneColor) — `dynamicRenderingUnusedAttachments`
// allows the rendering to have more attachments than the
// pipeline declares, and the per-frame
// `VkRenderingAttachmentInfo::imageView` for the unused slot is
// VK_NULL_HANDLE.
layout(location = 2) out vec4 outLayerMask;

const uint kSunShadowCascadeCount = 4u;
const uint kSunContactShadowMaxSteps = 12u;
const uint kAmbientOcclusionMaxSteps = 4u;
const uint kLocalPointLightShadowMaxSteps = 12u;
const float kHugeRayT = 1e20;

bool IsGlass(const uint materialIndex) {
    return materialIndex == 1u;
}

bool IsFluid(const uint materialIndex) {
    return materialIndex == 2u;
}

uint DecodeChunkVoxelMaterial(const ChunkDescriptor chunkDescriptor, const uvec3 localCoord) {
    if (any(greaterThanEqual(localCoord, chunkDescriptor.chunkExtentAndNonAir.xyz))) {
        return 0u;
    }

    const uint localIndex =
    localCoord.x +
    chunkDescriptor.chunkExtentAndNonAir.x *
    (localCoord.y + chunkDescriptor.chunkExtentAndNonAir.y * localCoord.z);
    if (localIndex >= chunkDescriptor.voxelDataInfo.y) {
        return 0u;
    }

    const uint wordIndex = chunkDescriptor.voxelDataInfo.x + localIndex / 4u;
    const uint shift = (localIndex & 3u) * 8u;
    return (chunkVoxelWords[wordIndex] >> shift) & 0xFFu;
}

uint ReadVoxelMaterial(const ivec3 worldPosition) {
    const int chunkSize = pushConstants.worldMinAndChunkSize.w;
    if (chunkSize <= 0) {
        return 0u;
    }

    const ivec3 worldMin = pushConstants.worldMinAndChunkSize.xyz;
    const ivec3 localWorldPosition = worldPosition - worldMin;
    if (any(lessThan(localWorldPosition, ivec3(0)))) {
        return 0u;
    }

    const uvec3 chunkGrid = pushConstants.chunkGridAndFlags.xyz;
    const uvec3 chunkCoord = uvec3(localWorldPosition) / uint(chunkSize);
    if (any(greaterThanEqual(chunkCoord, chunkGrid))) {
        return 0u;
    }

    const uint chunkIndex =
    chunkCoord.x +
    chunkGrid.x * (chunkCoord.y + chunkGrid.y * chunkCoord.z);
    const ChunkDescriptor chunkDescriptor = chunkDescriptors[chunkIndex];
    const ivec3 localChunkPosition = worldPosition - chunkDescriptor.chunkOrigin.xyz;
    if (any(lessThan(localChunkPosition, ivec3(0)))) {
        return 0u;
    }

    return DecodeChunkVoxelMaterial(chunkDescriptor, uvec3(localChunkPosition));
}

bool IsSunContactShadowOccluder(const uint materialIndex) {
    return materialIndex != 0u && !IsGlass(materialIndex);
}

bool IsAmbientOcclusionOccluder(const uint materialIndex) {
    return materialIndex != 0u && !IsGlass(materialIndex);
}

bool IsLocalPointLightShadowOccluder(const uint materialIndex) {
    return materialIndex != 0u && !IsGlass(materialIndex) && !IsFluid(materialIndex);
}

vec3 QuantizeVoxelFaceNormal(const vec3 normal) {
    return vec3(
    normal.x > 0.5 ? 1.0 : (normal.x < -0.5 ? -1.0 : 0.0),
    normal.y > 0.5 ? 1.0 : (normal.y < -0.5 ? -1.0 : 0.0),
    normal.z > 0.5 ? 1.0 : (normal.z < -0.5 ? -1.0 : 0.0));
}

vec3 ComputeStableVoxelFacePoint(const vec3 worldPosition, const vec3 normal) {
    const vec3 faceNormal = QuantizeVoxelFaceNormal(normal);
    const ivec3 surfaceVoxel = ivec3(floor(worldPosition - faceNormal * 0.5));
    const vec3 voxelMin = vec3(surfaceVoxel);
    const vec3 voxelMax = voxelMin + vec3(1.0);
    const float faceInset = 0.001;
    vec3 stablePoint = clamp(worldPosition, voxelMin + vec3(faceInset), voxelMax - vec3(faceInset));
    if (faceNormal.x > 0.5) {
        stablePoint.x = voxelMax.x - faceInset;
    } else if (faceNormal.x < -0.5) {
        stablePoint.x = voxelMin.x + faceInset;
    }
    if (faceNormal.y > 0.5) {
        stablePoint.y = voxelMax.y - faceInset;
    } else if (faceNormal.y < -0.5) {
        stablePoint.y = voxelMin.y + faceInset;
    }
    if (faceNormal.z > 0.5) {
        stablePoint.z = voxelMax.z - faceInset;
    } else if (faceNormal.z < -0.5) {
        stablePoint.z = voxelMin.z + faceInset;
    }
    return stablePoint;
}

vec3 ComputeRayStepTMax(
const vec3 origin,
const ivec3 currentVoxel,
const ivec3 stepDirection,
const vec3 rayDirection) {
    vec3 tMax = vec3(kHugeRayT);
    if (abs(rayDirection.x) > 0.00001) {
        const float nextBoundaryX = stepDirection.x > 0 ? float(currentVoxel.x + 1) : float(currentVoxel.x);
        tMax.x = (nextBoundaryX - origin.x) / rayDirection.x;
    }
    if (abs(rayDirection.y) > 0.00001) {
        const float nextBoundaryY = stepDirection.y > 0 ? float(currentVoxel.y + 1) : float(currentVoxel.y);
        tMax.y = (nextBoundaryY - origin.y) / rayDirection.y;
    }
    if (abs(rayDirection.z) > 0.00001) {
        const float nextBoundaryZ = stepDirection.z > 0 ? float(currentVoxel.z + 1) : float(currentVoxel.z);
        tMax.z = (nextBoundaryZ - origin.z) / rayDirection.z;
    }
    return tMax;
}

void BuildLocalPointLightSampleBasis(const vec3 direction, out vec3 tangentA, out vec3 tangentB) {
    const vec3 referenceAxis = abs(direction.y) < 0.95 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    tangentA = normalize(cross(referenceAxis, direction));
    tangentB = normalize(cross(direction, tangentA));
}

float TraceLocalPointLightShadowRay(
const vec3 stableFacePoint,
const vec3 faceNormal,
const vec3 rayDirection,
const float maxDistance,
const float shadowStrength,
const float bias) {
    const float surfaceOffset = max(bias, 0.02);
    // Push the ray origin *along the face normal only*, never along rayDirection. The
    // old `+ rayDirection * surfaceOffset` term pushed the start voxel into the next
    // neighbor when the light approached from a low angle, which made the DDA start
    // inside the local-light source's own voxel when the direction had a downward
    // component and the receiver was the *top* face of an opaque voxel. That first
    // DDA cell would then be classified as an occluder by the local-light policy and
    // produce a hard "self-shadow" patch on what should be a fully lit face. Pushing
    // along the face normal only keeps the start voxel strictly on the lit side of
    // the receiver plane regardless of light direction.
    const vec3 rayOrigin = stableFacePoint + faceNormal * surfaceOffset;
    ivec3 currentVoxel = ivec3(floor(rayOrigin));
    const ivec3 stepDirection = ivec3(
    rayDirection.x > 0.0 ? 1 : (rayDirection.x < 0.0 ? -1 : 0),
    rayDirection.y > 0.0 ? 1 : (rayDirection.y < 0.0 ? -1 : 0),
    rayDirection.z > 0.0 ? 1 : (rayDirection.z < 0.0 ? -1 : 0));
    const vec3 tDelta = vec3(
    abs(rayDirection.x) > 0.00001 ? abs(1.0 / rayDirection.x) : kHugeRayT,
    abs(rayDirection.y) > 0.00001 ? abs(1.0 / rayDirection.y) : kHugeRayT,
    abs(rayDirection.z) > 0.00001 ? abs(1.0 / rayDirection.z) : kHugeRayT);
    vec3 tMax = ComputeRayStepTMax(rayOrigin, currentVoxel, stepDirection, rayDirection);
    const uint maxSteps = uint(clamp(ceil(maxDistance * 2.0), 1.0, float(kLocalPointLightShadowMaxSteps)));
    float traveled = 0.0;

    for (uint stepIndex = 0u; stepIndex < kLocalPointLightShadowMaxSteps; ++stepIndex) {
        if (stepIndex >= maxSteps) {
            break;
        }

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                traveled = tMax.x;
                currentVoxel.x += stepDirection.x;
                tMax.x += tDelta.x;
            } else {
                traveled = tMax.z;
                currentVoxel.z += stepDirection.z;
                tMax.z += tDelta.z;
            }
        } else if (tMax.y < tMax.z) {
            traveled = tMax.y;
            currentVoxel.y += stepDirection.y;
            tMax.y += tDelta.y;
        } else {
            traveled = tMax.z;
            currentVoxel.z += stepDirection.z;
            tMax.z += tDelta.z;
        }

        if (traveled >= maxDistance) {
            break;
        }

        const uint hitMaterial = ReadVoxelMaterial(currentVoxel);
        if (IsLocalPointLightShadowOccluder(hitMaterial)) {
            return 1.0 - shadowStrength;
        }
    }

    return 1.0;
}

float ComputeSunContactVisibility(const vec3 worldPosition, const vec3 normal, const vec3 sunDirection) {
    const float contactStrength = clamp(sceneLighting.sunContactShadowParams.x, 0.0, 1.0);
    const float maxDistance = max(sceneLighting.sunContactShadowParams.y, 0.0);
    if (contactStrength <= 0.0 || maxDistance <= 0.0) {
        return 1.0;
    }

    const vec3 rayDirection = normalize(sunDirection);
    const vec3 rayOrigin = worldPosition + normal * 0.05 + rayDirection * 0.05;
    ivec3 currentVoxel = ivec3(floor(rayOrigin));
    const ivec3 stepDirection = ivec3(
    rayDirection.x > 0.0 ? 1 : (rayDirection.x < 0.0 ? -1 : 0),
    rayDirection.y > 0.0 ? 1 : (rayDirection.y < 0.0 ? -1 : 0),
    rayDirection.z > 0.0 ? 1 : (rayDirection.z < 0.0 ? -1 : 0));
    const vec3 tDelta = vec3(
    abs(rayDirection.x) > 0.00001 ? abs(1.0 / rayDirection.x) : kHugeRayT,
    abs(rayDirection.y) > 0.00001 ? abs(1.0 / rayDirection.y) : kHugeRayT,
    abs(rayDirection.z) > 0.00001 ? abs(1.0 / rayDirection.z) : kHugeRayT);
    vec3 tMax = ComputeRayStepTMax(rayOrigin, currentVoxel, stepDirection, rayDirection);
    float traveled = 0.0;

    for (uint stepIndex = 0u; stepIndex < kSunContactShadowMaxSteps; ++stepIndex) {
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                traveled = tMax.x;
                currentVoxel.x += stepDirection.x;
                tMax.x += tDelta.x;
            } else {
                traveled = tMax.z;
                currentVoxel.z += stepDirection.z;
                tMax.z += tDelta.z;
            }
        } else if (tMax.y < tMax.z) {
            traveled = tMax.y;
            currentVoxel.y += stepDirection.y;
            tMax.y += tDelta.y;
        } else {
            traveled = tMax.z;
            currentVoxel.z += stepDirection.z;
            tMax.z += tDelta.z;
        }

        if (traveled > maxDistance) {
            break;
        }

        const uint hitMaterial = ReadVoxelMaterial(currentVoxel);
        if (IsSunContactShadowOccluder(hitMaterial)) {
            const float distanceFade = 1.0 - clamp(traveled / maxDistance, 0.0, 1.0);
            return 1.0 - contactStrength * distanceFade;
        }
    }

    return 1.0;
}

float TraceAmbientOcclusionRay(
const vec3 rayOrigin,
const vec3 rayDirection,
const float maxDistance,
const uint maxSteps) {
    if (maxDistance <= 0.0) {
        return 0.0;
    }

    ivec3 currentVoxel = ivec3(floor(rayOrigin));
    const ivec3 stepDirection = ivec3(
    rayDirection.x > 0.0 ? 1 : (rayDirection.x < 0.0 ? -1 : 0),
    rayDirection.y > 0.0 ? 1 : (rayDirection.y < 0.0 ? -1 : 0),
    rayDirection.z > 0.0 ? 1 : (rayDirection.z < 0.0 ? -1 : 0));
    const vec3 tDelta = vec3(
    abs(rayDirection.x) > 0.00001 ? abs(1.0 / rayDirection.x) : kHugeRayT,
    abs(rayDirection.y) > 0.00001 ? abs(1.0 / rayDirection.y) : kHugeRayT,
    abs(rayDirection.z) > 0.00001 ? abs(1.0 / rayDirection.z) : kHugeRayT);
    vec3 tMax = ComputeRayStepTMax(rayOrigin, currentVoxel, stepDirection, rayDirection);
    float traveled = 0.0;

    for (uint stepIndex = 0u; stepIndex < kAmbientOcclusionMaxSteps; ++stepIndex) {
        if (stepIndex >= maxSteps) {
            break;
        }

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                traveled = tMax.x;
                currentVoxel.x += stepDirection.x;
                tMax.x += tDelta.x;
            } else {
                traveled = tMax.z;
                currentVoxel.z += stepDirection.z;
                tMax.z += tDelta.z;
            }
        } else if (tMax.y < tMax.z) {
            traveled = tMax.y;
            currentVoxel.y += stepDirection.y;
            tMax.y += tDelta.y;
        } else {
            traveled = tMax.z;
            currentVoxel.z += stepDirection.z;
            tMax.z += tDelta.z;
        }

        if (traveled > maxDistance) {
            break;
        }

        const uint hitMaterial = ReadVoxelMaterial(currentVoxel);
        if (IsAmbientOcclusionOccluder(hitMaterial)) {
            const float distanceFade = 1.0 - clamp(traveled / maxDistance, 0.0, 1.0);
            return distanceFade * distanceFade;
        }
    }

    return 0.0;
}

void BuildSurfaceTangentBasis(const vec3 normal, out vec3 tangentA, out vec3 tangentB) {
    const vec3 absNormal = abs(normal);
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        tangentA = vec3(1.0, 0.0, 0.0);
        tangentB = vec3(0.0, 0.0, 1.0);
    } else if (absNormal.x >= absNormal.z) {
        tangentA = vec3(0.0, 1.0, 0.0);
        tangentB = vec3(0.0, 0.0, 1.0);
    } else {
        tangentA = vec3(1.0, 0.0, 0.0);
        tangentB = vec3(0.0, 1.0, 0.0);
    }
}

float SampleAmbientOcclusionDirection(
const vec3 worldPosition,
const vec3 normal,
const vec3 direction,
const float radius,
const uint maxSteps) {
    const vec3 rayDirection = normalize(direction);
    const vec3 rayOrigin = worldPosition + normal * 0.14 + rayDirection * 0.03;
    return TraceAmbientOcclusionRay(rayOrigin, rayDirection, radius, maxSteps);
}

float ComputeAmbientOcclusionVisibility(const vec3 worldPosition, const vec3 normal) {
    const float strength = clamp(sceneLighting.ambientOcclusionParams.x, 0.0, 1.0);
    const float radius = max(sceneLighting.ambientOcclusionParams.y, 0.0);
    const float minVisibility = clamp(sceneLighting.ambientOcclusionParams.z, 0.0, 1.0);
    if (strength <= 0.0 || radius <= 0.0) {
        return 1.0;
    }

    vec3 tangentA = vec3(1.0, 0.0, 0.0);
    vec3 tangentB = vec3(0.0, 0.0, 1.0);
    BuildSurfaceTangentBasis(normal, tangentA, tangentB);

    const float normalWeight = 0.5;
    const float sideWeight = 0.25;
    const float normalLift = 0.90;
    const float sideSpread = 0.55;
    float occlusion = 0.0;
    float weight = 0.0;
    // Three-tap AOCC: the surface normal plus two side rays. The previous five-tap
    // configuration sampled tangentA± and tangentB± separately, which doubled the
    // fragment budget on AOCC for no visible quality difference at the tuned
    // strength/radius. The new direction set keeps a strong normal axis and a single
    // lateral pair, which still produces a readable cavity layer.
    occlusion += SampleAmbientOcclusionDirection(worldPosition, normal, normal, radius, kAmbientOcclusionMaxSteps) * normalWeight;
    weight += normalWeight;
    occlusion += SampleAmbientOcclusionDirection(worldPosition, normal, normal * normalLift + tangentA * sideSpread, radius, kAmbientOcclusionMaxSteps) * sideWeight;
    occlusion += SampleAmbientOcclusionDirection(worldPosition, normal, normal * normalLift - tangentA * sideSpread, radius, kAmbientOcclusionMaxSteps) * sideWeight;
    weight += sideWeight * 2.0;

    const float normalizedOcclusion = clamp(occlusion / max(weight, 0.0001), 0.0, 1.0);
    return clamp(1.0 - strength * normalizedOcclusion, minVisibility, 1.0);
}

float ComputeLocalPointLightVisibility(
const vec3 worldPosition,
const vec3 normal) {
    const float shadowStrength = clamp(sceneLighting.localPointLightParams.z, 0.0, 1.0);
    if (shadowStrength <= 0.0) {
        return 1.0;
    }

    const float bias = max(sceneLighting.localPointLightParams.w, 0.0);
    const vec3 stableFacePoint = ComputeStableVoxelFacePoint(worldPosition, normal);
    const vec3 faceNormal = QuantizeVoxelFaceNormal(normal);
    // Local-light visibility must stay tied to the owning voxel face without collapsing to one constant value
    // for the whole face. A stabilized face-plane point keeps close-range traces continuous across a face while
    // still preventing the old sub-voxel DDA leaks from raw interpolated boundary positions.
    const vec3 lightCenter = sceneLighting.localPointLightPositionAndRadius.xyz;
    const vec3 centerDelta = lightCenter - stableFacePoint;
    const float centerDistanceSq = dot(centerDelta, centerDelta);
    if (centerDistanceSq <= 0.000001) {
        return 1.0;
    }

    const float centerDistance = sqrt(centerDistanceSq);
    const vec3 centerDirection = centerDelta / centerDistance;
    float visibility = TraceLocalPointLightShadowRay(
    stableFacePoint,
    faceNormal,
    centerDirection,
    centerDistance,
    shadowStrength,
    bias) * 0.4;
    float weight = 0.4;

    const float sourceRadius = max(sceneLighting.localPointLightParams.y, 0.0);
    const float sampleRadius = min(sourceRadius * 0.6, centerDistance * 0.35);
    if (sampleRadius <= 0.0001) {
        return visibility / max(weight, 0.0001);
    }

    vec3 tangentA = vec3(1.0, 0.0, 0.0);
    vec3 tangentB = vec3(0.0, 0.0, 1.0);
    BuildLocalPointLightSampleBasis(centerDirection, tangentA, tangentB);
    const vec2 sampleOffsets[4] = vec2[4](
    vec2(1.0, 0.0),
    vec2(-1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, -1.0));

    for (uint sampleIndex = 0u; sampleIndex < 4u; ++sampleIndex) {
        const vec3 sampleLightPosition =
        lightCenter +
        tangentA * (sampleOffsets[sampleIndex].x * sampleRadius) +
        tangentB * (sampleOffsets[sampleIndex].y * sampleRadius);
        const vec3 sampleDelta = sampleLightPosition - stableFacePoint;
        const float sampleDistanceSq = dot(sampleDelta, sampleDelta);
        if (sampleDistanceSq <= 0.000001) {
            continue;
        }

        const float sampleDistance = sqrt(sampleDistanceSq);
        const vec3 sampleDirection = sampleDelta / sampleDistance;
        visibility += TraceLocalPointLightShadowRay(
        stableFacePoint,
        faceNormal,
        sampleDirection,
        sampleDistance,
        shadowStrength,
        bias) * 0.15;
        weight += 0.15;
    }

    return visibility / max(weight, 0.0001);
}

vec3 SampleEnvironmentDiffuse(const vec3 normal) {
    const float up = clamp(normal.y, -1.0, 1.0);
    const float skyWeight = smoothstep(-0.25, 0.85, up);
    const float groundWeight = smoothstep(0.35, -0.75, up) * 0.65;
    const float horizonWeight = pow(1.0 - abs(up), 2.0) * 0.55;
    const vec3 environment =
    sceneLighting.skyColorAndFogDensity.rgb * skyWeight +
    sceneLighting.groundColorAndFogMax.rgb * groundWeight +
    sceneLighting.horizonColorAndFogStart.rgb * (horizonWeight + 0.08);
    return environment * max(sceneLighting.postProcess.y, 0.0);
}

vec3 ApplyToneMap(const vec3 linearColor) {
    const uint toneMapOperator = uint(sceneLighting.postProcess.z + 0.5);
    if (toneMapOperator == 0u) {
        return clamp(linearColor, 0.0, 1.0);
    }
    if (toneMapOperator == 1u) {
        return linearColor / (1.0 + max(linearColor, vec3(0.0)));
    }

    const vec3 a = linearColor * (2.51 * linearColor + 0.03);
    const vec3 b = linearColor * (2.43 * linearColor + 0.59) + 0.14;
    return clamp(a / b, 0.0, 1.0);
}

vec3 ApplyColorGrading(const vec3 mappedColor) {
    const float whitePoint = clamp(sceneLighting.colorGrading.x, 0.25, 4.0);
    const float contrast = clamp(sceneLighting.colorGrading.y, 0.0, 2.0);
    const float saturation = clamp(sceneLighting.colorGrading.z, 0.0, 2.0);
    const float lift = clamp(sceneLighting.colorGrading.w, -0.25, 0.25);
    const vec3 normalizedColor = mappedColor / whitePoint;
    const float luma = dot(normalizedColor, vec3(0.2126, 0.7152, 0.0722));
    const vec3 saturatedColor = mix(vec3(luma), normalizedColor, saturation);
    return clamp((saturatedColor - vec3(0.5)) * contrast + vec3(0.5 + lift), 0.0, 1.0);
}

float DistributionGGX(const float nDotH, const float roughness) {
    const float alpha = roughness * roughness;
    const float alphaSq = alpha * alpha;
    const float denom = max(nDotH * nDotH * (alphaSq - 1.0) + 1.0, 0.0001);
    return alphaSq / (3.14159265 * denom * denom);
}

float GeometrySchlickGGX(const float nDotX, const float roughness) {
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0;
    return nDotX / max(nDotX * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(const float nDotV, const float nDotL, const float roughness) {
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

vec3 FresnelSchlick(const float cosTheta, const vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
}

vec3 EvaluateDirectLighting(
const vec3 lightDirection,
const vec3 lightRadiance,
const vec3 normal,
const vec3 viewDirection,
const vec3 albedo,
const float roughness,
const float metallic,
const float reflectance,
const float directDiffuseStrength,
const float diffuseWrap) {
    const float nDotL = max(dot(normal, lightDirection), 0.0);
    const float nDotV = max(dot(normal, viewDirection), 0.0);
    const float wrappedDiffuse = clamp((dot(normal, lightDirection) + diffuseWrap) / (1.0 + diffuseWrap), 0.0, 1.0);
    const vec3 halfVector = normalize(lightDirection + viewDirection + vec3(0.0001));
    const float nDotH = max(dot(normal, halfVector), 0.0);
    const float hDotV = max(dot(halfVector, viewDirection), 0.0);
    const vec3 f0 = mix(vec3(0.16 * reflectance * reflectance), albedo, metallic);
    const vec3 fresnel = FresnelSchlick(hDotV, f0);
    const float distribution = DistributionGGX(nDotH, roughness);
    const float geometry = GeometrySmith(nDotV, nDotL, roughness);
    const vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotL * nDotV, 0.0001);
    const vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo;
    return diffuse * lightRadiance * wrappedDiffuse * directDiffuseStrength + specular * lightRadiance * nDotL;
}

vec3 ComputeLocalPointLightDirect(
const vec3 worldPosition,
const vec3 normal,
const vec3 viewDirection,
const vec3 albedo,
const float roughness,
const float metallic,
const float reflectance,
const float directDiffuseStrength,
const float localPointLightVisibility) {
    if (sceneLighting.localPointLightParams.x <= 0.5) {
        return vec3(0.0);
    }

    const float radius = max(sceneLighting.localPointLightPositionAndRadius.w, 0.0);
    if (radius <= 0.0) {
        return vec3(0.0);
    }

    const vec3 lightDelta = sceneLighting.localPointLightPositionAndRadius.xyz - worldPosition;
    const float distanceSq = dot(lightDelta, lightDelta);
    const float radiusSq = radius * radius;
    if (distanceSq <= 0.000001 || distanceSq >= radiusSq) {
        return vec3(0.0);
    }

    const float distance = sqrt(distanceSq);
    const vec3 lightDirection = lightDelta / distance;
    if (dot(normal, lightDirection) <= 0.0) {
        return vec3(0.0);
    }

    const float normalizedDistance = clamp(distance / radius, 0.0, 1.0);
    const float rangeFade = 1.0 - smoothstep(0.75, 1.0, normalizedDistance);
    const float sourceRadius = max(sceneLighting.localPointLightParams.y, 0.05);
    const float attenuation = rangeFade / max(distanceSq, sourceRadius * sourceRadius);
    if (attenuation <= 0.00001) {
        return vec3(0.0);
    }

    // 1.5 anti-flicker: caller is expected to have already blended
    // `localPointLightVisibility` with the previous-frame history
    // (see `main()` below). The DDA is no longer invoked here
    // because that would either do the work twice or require
    // duplicating the early-out logic; the caller's `main()` keeps
    // the unconditional pre-blend invocation.
    const float visibility = localPointLightVisibility;
    const vec3 lightRadiance =
    sceneLighting.localPointLightColorAndIntensity.rgb *
    sceneLighting.localPointLightColorAndIntensity.w *
    attenuation *
    visibility;
    return EvaluateDirectLighting(
    lightDirection,
    lightRadiance,
    normal,
    viewDirection,
    albedo,
    roughness,
    metallic,
    reflectance,
    directDiffuseStrength,
    0.0);
}

float GetCameraViewDepth(const vec3 worldPosition) {
    const float near = pushConstants.cameraPosition.w;
    const float far = pushConstants.cameraForward.w;
    const float z = gl_FragCoord.z;
    return near * far / (far - z * (far - near));
}

uint SelectSunShadowCascadeByViewDepth(const float viewDepth) {
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.x) {
        return 0u;
    }
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.y) {
        return 1u;
    }
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.z) {
        return 2u;
    }
    return 3u;
}

float GetSunShadowCascadeNearDepth(const uint cascadeIndex) {
    if (cascadeIndex == 0u) {
        return max(sceneLighting.shadowCascadeBlendParams.y, 0.0);
    }
    return sceneLighting.shadowCascadeDepthSplits[cascadeIndex - 1u];
}

float ComputeSunShadowCascadeBlendWeight(const float viewDepth, const uint cascadeIndex) {
    const float blendFraction = clamp(sceneLighting.shadowCascadeBlendParams.x, 0.0, 0.5);
    if (blendFraction <= 0.0 || cascadeIndex + 1u >= kSunShadowCascadeCount) {
        return 0.0;
    }

    const float cascadeNearDepth = GetSunShadowCascadeNearDepth(cascadeIndex);
    const float cascadeFarDepth = sceneLighting.shadowCascadeDepthSplits[cascadeIndex];
    const float cascadeRange = max(cascadeFarDepth - cascadeNearDepth, 0.0001);
    const float blendStartDepth = max(cascadeFarDepth - cascadeRange * blendFraction, cascadeNearDepth);
    return smoothstep(blendStartDepth, cascadeFarDepth, viewDepth);
}

vec3 GetSunShadowCascadeDebugColor(const uint cascadeIndex) {
    if (cascadeIndex == 0u) {
        return vec3(0.15, 0.75, 1.0);
    }
    if (cascadeIndex == 1u) {
        return vec3(0.35, 1.0, 0.35);
    }
    if (cascadeIndex == 2u) {
        return vec3(1.0, 0.85, 0.25);
    }
    return vec3(1.0, 0.35, 0.25);
}

vec2 SampleSunShadowCascade(
const uint cascadeIndex,
const vec3 receiverPosition,
const float receiverDepthBias,
const float filterRadius,
const float shadowStrength) {
    const vec4 shadowClip = sceneLighting.sunShadowViewProjections[cascadeIndex] * vec4(receiverPosition, 1.0);
    const float shadowW = max(shadowClip.w, 0.0001);
    const vec3 shadowNdc = shadowClip.xyz / shadowW;
    const vec2 shadowUv = shadowNdc.xy * 0.5 + 0.5;

    if (shadowUv.x <= 0.0 || shadowUv.x >= 1.0 ||
    shadowUv.y <= 0.0 || shadowUv.y >= 1.0 ||
    shadowNdc.z <= 0.0 || shadowNdc.z >= 1.0) {
        return vec2(1.0, 0.0);
    }

    if (filterRadius <= 0.0) {
        const float lit = texture(sunShadowMap, vec4(shadowUv, float(cascadeIndex), shadowNdc.z - receiverDepthBias));
        return vec2(mix(1.0 - shadowStrength, 1.0, lit), 1.0);
    }

    // A small weighted PCF kernel is still cheap enough for the current CSM baseline,
    // but hides the nearest-neighbor texel staircase better than the old 3x3 box filter.
    const vec2 texelSize = 1.0 / vec2(textureSize(sunShadowMap, 0).xy);
    float litAccum = 0.0;
    float weightAccum = 0.0;
    const float pcfStepScale = filterRadius * 0.75;
    for (int offsetY = -2; offsetY <= 2; ++offsetY) {
        for (int offsetX = -2; offsetX <= 2; ++offsetX) {
            const float sampleWeight =
            (3.0 - abs(float(offsetX))) *
            (3.0 - abs(float(offsetY)));
            const vec2 sampleUv = shadowUv + vec2(offsetX, offsetY) * texelSize * pcfStepScale;
            litAccum += texture(sunShadowMap, vec4(sampleUv, float(cascadeIndex), shadowNdc.z - receiverDepthBias)) * sampleWeight;
            weightAccum += sampleWeight;
        }
    }
    const float lit = litAccum / max(weightAccum, 1.0);
    return vec2(mix(1.0 - shadowStrength, 1.0, lit), 1.0);
}

vec4 ComputeSunShadowSample(const vec3 worldPosition, const vec3 normal) {
    const float viewDepth = GetCameraViewDepth(worldPosition);
    const uint cascadeIndex = SelectSunShadowCascadeByViewDepth(viewDepth);
    const float shadowStrength = clamp(sceneLighting.sunShadowParams.x, 0.0, 1.0);
    if (shadowStrength <= 0.0) {
        const float contactVisibility = ComputeSunContactVisibility(
        worldPosition,
        normal,
        normalize(sceneLighting.sunDirectionAndWrap.xyz));
        return vec4(contactVisibility, contactVisibility < 0.999 ? 1.0 : 0.0, float(cascadeIndex), contactVisibility);
    }

    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const float depthBias = max(sceneLighting.sunShadowParams.y, 0.0);
    const float normalBias = max(sceneLighting.sunShadowParams.z, 0.0);
    // The PCF kernel is `5 * 5` taps with `pcfStepScale = filterRadius * 0.75` texels
    // per tap. Preset values land in [1.10, 1.50] (roughly ±1.5 texels from center),
    // which is the visual sweet spot. The debug-ladder `H/K` controls let the user
    // push the authored filter radius up to 8.0, which expands the kernel to ±12
    // texels (roughly 50% of a cascade's texel budget at 2048). That over-blooms
    // adjacent casters into each other and starts to *erase* the contact-shadow
    // detail. Clamp to a sane upper bound here so the runtime never produces a
    // visually broken filter, even with the debug controls at their extreme.
    const float filterRadius = clamp(sceneLighting.sunShadowParams.w, 0.0, 2.0);
    const float nDotL = clamp(dot(normal, sunDirection), 0.0, 1.0);
    if (nDotL <= 0.02) {
        return vec4(1.0, 1.0, float(cascadeIndex), 1.0);
    }

    const float shadowSlope = 1.0 - nDotL;
    // Keep the authored bias controls stable, but make them respond to the sun angle so
    // acne on grazing receivers does not require the same brute-force offset on flat tops.
    const float receiverDepthBias = depthBias * (1.0 + shadowSlope * 2.0);
    const float receiverNormalBias = normalBias * mix(0.25, 1.0, shadowSlope);
    const float receiverLightBias = max(normalBias * 0.5, depthBias * 4.0);
    const vec3 receiverPosition =
    worldPosition +
    normal * receiverNormalBias +
    sunDirection * receiverLightBias;
    vec2 shadowSample = SampleSunShadowCascade(
    cascadeIndex,
    receiverPosition,
    receiverDepthBias,
    filterRadius,
    shadowStrength);
    const float cascadeBlendWeight = ComputeSunShadowCascadeBlendWeight(viewDepth, cascadeIndex);
    if (cascadeBlendWeight > 0.0 && cascadeIndex + 1u < kSunShadowCascadeCount) {
        const vec2 nextShadowSample = SampleSunShadowCascade(
        cascadeIndex + 1u,
        receiverPosition,
        receiverDepthBias,
        filterRadius,
        shadowStrength);
        if (nextShadowSample.y > 0.5) {
            shadowSample.x = shadowSample.y > 0.5
            ? mix(shadowSample.x, nextShadowSample.x, cascadeBlendWeight)
            : nextShadowSample.x;
            shadowSample.y = 1.0;
        }
    }
    const float contactVisibility = ComputeSunContactVisibility(receiverPosition, normal, sunDirection);
    shadowSample.x *= contactVisibility;
    const float anyShadowCoverage = shadowSample.y > 0.5 || contactVisibility < 0.999 ? 1.0 : 0.0;
    return vec4(shadowSample.x, anyShadowCoverage, float(cascadeIndex), contactVisibility);
}

void main() {
    const MaterialVisual material = materials[inMaterialIndex];
    const vec3 normal = normalize(inNormal);
    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const vec3 sunColor = sceneLighting.sunColorAndIntensity.rgb * sceneLighting.sunColorAndIntensity.w;
    const vec4 sunShadowSample = ComputeSunShadowSample(inWorldPosition, normal);
    const float sunVisibility = sunShadowSample.x;
    const bool shadowCovered = sunShadowSample.y > 0.5;
    const uint sunShadowCascadeIndex = uint(sunShadowSample.z + 0.5);
    const float sunContactVisibility = sunShadowSample.w;
    const float sunShadowViewDepth = GetCameraViewDepth(inWorldPosition);
    const vec3 shadowedSunColor = sunColor * sunVisibility;
    const vec3 viewDirection = normalize(pushConstants.cameraPosition.xyz - inWorldPosition + vec3(0.0001));
    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const float ao = clamp(material.surface.x, 0.0, 1.0);
    const float roughness = clamp(material.surface.y, 0.045, 1.0);
    const float metallic = clamp(material.surface.z, 0.0, 1.0);
    const float reflectance = clamp(material.surface.w, 0.0, 1.0);
    const float ambientStrength = clamp(material.shading.z, 0.0, 1.0);
    const float directDiffuseStrength = clamp(material.shading.w, 0.0, 1.0);
    const float nDotV = max(dot(normal, viewDirection), 0.0);
    const float wrappedDiffuse = clamp((dot(normal, sunDirection) + diffuseWrap) / (1.0 + diffuseWrap), 0.0, 1.0);
    const vec3 albedo = material.baseColor.rgb;
    const float ambientOcclusion = mix(0.35, 1.0, ao);
    // This is the cheap meshing-side local sky-visibility term, not screen-space AO/GI.
    const float geometryAmbientVisibility = clamp(inAmbientVisibility, 0.0, 1.0);
    const float localAmbientOcclusionVisibility = ComputeAmbientOcclusionVisibility(inWorldPosition, normal);
    // 1.5 anti-flicker: pre-blend the per-layer values with the
    // previous-frame history. The blend is `mix(raw, history, blend)`
    // with `blend = taaLayerHistoryParams.w` (default 0.4) — the
    // current frame's lighting uses the blended value, while the
    // raw current-frame value is what `outLayerMask` writes to the
    // next frame's history (blend-at-read, not blend-at-write, so
    // each frame's contribution to the temporal filter is uniform
    // — see `agent/memory.md §10.19` for the contract).
    // `taaLayerHistoryParams.z` is the history-valid flag: if 0,
    // the history is not yet initialised (post-swapchain-recreate /
    // first frame), and we fall through to the raw current value
    // without sampling. `taaLayerHistoryParams.x/y` are the
    // texel-size reciprocals in the layer-history texel space (the
    // layer history matches the swapchain extent, same as the
    // colour history).
    const bool layerHistoryValid = sceneLighting.taaLayerHistoryParams.z > 0.5;
    const vec2 layerTexelSize = sceneLighting.taaLayerHistoryParams.xy;
    const float layerBlend = clamp(sceneLighting.taaLayerHistoryParams.w, 0.0, 1.0);

    vec2 layerUv = gl_FragCoord.xy * layerTexelSize;
    if (layerHistoryValid) {
        const vec4 prevClip = sceneLighting.prevViewProjectionMatrix *
            vec4(inWorldPosition, 1.0);
        if (prevClip.w > 0.0001) {
            const vec2 reprojectedUv = prevClip.xy / prevClip.w * 0.5 + 0.5;
            if (all(greaterThanEqual(reprojectedUv, vec2(0.0))) &&
                all(lessThanEqual(reprojectedUv, vec2(1.0)))) {
                layerUv = reprojectedUv;
            }
        }
    }

    const vec4 historyLayerSample = layerHistoryValid
        ? texture(layerHistory, layerUv)
        : vec4(1.0, 1.0, 1.0, 1.0);
    // 1.5 anti-flicker: also blend local-point-light visibility.
    // Always compute the DDA at the top of main() so the raw value
    // is available for `outLayerMask` write (this is a small cost
    // increase vs the previous "compute on demand" approach, but
    // the DDA is only 12 steps and the layer mask is the whole
    // point of this slice). The blended value goes into
    // `ComputeLocalPointLightDirect` below.
    const float rawLocalPointLightVisibility = ComputeLocalPointLightVisibility(inWorldPosition, normal);
    // 1.5 anti-flicker: AOCC is the per-pixel ray-cast cavity term;
    // smoothing it removes the per-frame jitter that's most visible
    // in enclosed niches. The blended value goes into
    // `ambientVisibility` below.
    const float blendedLocalAmbientOcclusionVisibility = mix(localAmbientOcclusionVisibility, historyLayerSample.y, layerBlend);
    // 1.5 — sun contact shadow (CTSH, in `.x` of the history sample)
    // is currently written to history but not yet blended in main
    // lighting. Smoothing CTSH needs `ComputeSunShadowSample` to
    // separate the contact visibility from the cascade shadow so
    // the blended contact can be applied at the end (the function
    // currently multiplies them internally). That's a follow-up
    // slice — the data is already in history, so the future
    // change is local to `main()` and the call to
    // `ComputeSunShadowSample`. Tracking the `.x` history read
    // here so the value doesn't go unused in the next slice
    // (compiler is fine with the read, the value will just
    // participate in the future blend).
    const float blendedSunContactVisibility = mix(sunContactVisibility, historyLayerSample.x, layerBlend);
    const float blendedLocalPointLightVisibility = mix(rawLocalPointLightVisibility, historyLayerSample.z, layerBlend);
    const float ambientVisibility = geometryAmbientVisibility * blendedLocalAmbientOcclusionVisibility;
    const vec3 ambient =
    SampleEnvironmentDiffuse(normal) *
    albedo *
    ambientStrength *
    ambientOcclusion *
    ambientVisibility;
    const vec3 directSun = EvaluateDirectLighting(
    sunDirection,
    shadowedSunColor,
    normal,
    viewDirection,
    albedo,
    roughness,
    metallic,
    reflectance,
    directDiffuseStrength,
    diffuseWrap);
    const vec3 localDirect = ComputeLocalPointLightDirect(
    inWorldPosition,
    normal,
    viewDirection,
    albedo,
    roughness,
    metallic,
    reflectance,
    directDiffuseStrength,
    blendedLocalPointLightVisibility);
    const float grazing = pow(1.0 - nDotV, 5.0);
    const vec3 mediumTint = material.medium.rgb;
    const vec3 grazingTint = mediumTint * material.medium.w * grazing * (1.0 - metallic) * 0.12;
    vec3 color = ambient + directSun + localDirect + grazingTint;

    if (material.medium.w > 0.0) {
        float transmission = material.medium.w * mix(0.35, 1.0, wrappedDiffuse);
        if (IsGlass(inMaterialIndex)) {
            transmission *= mix(0.75, 1.15, grazing);
        } else if (IsFluid(inMaterialIndex)) {
            transmission *= 0.60 + grazing * 0.40;
        }
        color += sceneLighting.horizonColorAndFogStart.rgb * mediumTint * transmission;
    }

    if (material.shading.y > 0.0) {
        color += albedo * material.shading.y;
    }

    const float viewDistance = length(pushConstants.cameraPosition.xyz - inWorldPosition);
    const float fogDensity = max(sceneLighting.skyColorAndFogDensity.w, 0.0);
    const float fogStart = max(sceneLighting.horizonColorAndFogStart.w, 0.0);
    const float fogMax = clamp(sceneLighting.groundColorAndFogMax.w, 0.0, 1.0);
    const vec3 fogColor = mix(
    sceneLighting.horizonColorAndFogStart.rgb,
    sceneLighting.skyColorAndFogDensity.rgb,
    clamp(normal.y * 0.5 + 0.5, 0.0, 1.0));
    const float fog = clamp(max(viewDistance - fogStart, 0.0) * fogDensity, 0.0, fogMax) * material.shading.x;
    const uint lightingDebugView = uint(sceneLighting.postProcess.w + 0.5);

    if (lightingDebugView == 1u) {
        color = ambient;
    } else if (lightingDebugView == 2u) {
        color = directSun + localDirect + grazingTint;
    } else if (lightingDebugView == 3u) {
        color = localDirect;
    } else if (lightingDebugView == 4u) {
        OUT_COLOR = vec4(shadowCovered ? vec3(sunVisibility) : vec3(1.0, 0.15, 0.10), 1.0);
        return;
    } else if (lightingDebugView == 5u) {
        vec3 cascadeColor = GetSunShadowCascadeDebugColor(sunShadowCascadeIndex);
        const float cascadeBlendWeight = ComputeSunShadowCascadeBlendWeight(sunShadowViewDepth, sunShadowCascadeIndex);
        if (cascadeBlendWeight > 0.0 && sunShadowCascadeIndex + 1u < kSunShadowCascadeCount) {
            cascadeColor = mix(
            cascadeColor,
            GetSunShadowCascadeDebugColor(sunShadowCascadeIndex + 1u),
            cascadeBlendWeight);
        }
        OUT_COLOR = vec4(shadowCovered ? mix(cascadeColor * 0.28, cascadeColor, sunVisibility) : vec3(1.0, 0.15, 0.10), 1.0);
        return;
    } else if (lightingDebugView == 6u) {
        color = vec3(sunContactVisibility);
    } else if (lightingDebugView == 7u) {
        color = vec3(localAmbientOcclusionVisibility);
    } else if (lightingDebugView == 8u) {
        color = vec3(fog);
    }

    if (lightingDebugView != 8u) {
        color = mix(color, fogColor, fog);
    }
    const vec3 linearColor = color;
    color *= max(sceneLighting.postProcess.x, 0.0);
    color = ApplyToneMap(color);
    color = ApplyColorGrading(color);

    // 1.5 anti-flicker: write the *raw* per-layer values to
    // `outLayerMask` so the next frame's voxel pass can sample them
    // as history. Layout: R = sun contact shadow visibility (raw,
    // not yet smoothed in this slice — see `agent/memory.md §10.19`
    // for the rationale and follow-up work), G = local ambient
    // occlusion visibility (raw), B = local point light visibility
    // (raw), A = 1.0 (reserved). The actual temporal blend is
    // applied above in the lighting block via the `blended*`
    // variables; the next frame's voxel pass will read the values
    // we wrote here as the previous-frame input.
    outLayerMask = vec4(sunContactVisibility, localAmbientOcclusionVisibility, rawLocalPointLightVisibility, 1.0);

#ifdef TAA_ENABLED
    outSceneColor = vec4(linearColor, material.baseColor.a);
#else
    outColor = vec4(color, material.baseColor.a);
#endif
}
