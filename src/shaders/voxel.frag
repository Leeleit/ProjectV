#version 460

#include "common/common_constants.glsl"

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
    vec4 sunContactShadowParams;
    vec4 ambientOcclusionParams;
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 localPointLightPositionAndRadius;
    vec4 localPointLightColorAndIntensity;
    vec4 localPointLightParams;
    vec4 taaParams;
    mat4 prevViewProjectionMatrix;
    vec4 taaHistoryParams;
    vec4 taaLayerHistoryParams;
    vec4 vctParams;
    vec4 vctSpecularParams;
} sceneLighting;

layout(set = 0, binding = 4, std430) readonly buffer PackedChunkVoxelPayload {
    uint chunkVoxelWords[];
};


layout(set = 0, binding = 6) uniform sampler2D layerHistory;

layout(set = 0, binding = 11) uniform sampler3D vctClipmap;
// EVIL: binding 12 = volumetricFog sampler3D FRAGMENT. Per TODO §5.4 Wronski 2014 fog
// consume. Bound to fallback 1x1x1 RGBA16F dummy when PROJECTV_FOG=ON not set.
layout(set = 0, binding = 12) uniform sampler3D volumetricFog;

#ifdef VOXEL_RTX_ENABLED
#extension GL_EXT_ray_query : require
// EVIL: binding 13 = RTX top-level acceleration structure (TLAS) for ray-query
// smooth specular GI per Stage 5.2. Bound to scene TLAS when RTX env-gate ON;
// otherwise (non-RTX compile) binding slot is unused. Per
// docs/VulkanSDK-Linux-Docs-1.4.350.1/chunked_spec/chap63.html the
// accelerationStructureEXT uniform is GLSL-side; C++ side uses
// VkAccelerationStructureKHR via VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR.
layout(set = 0, binding = 13) uniform accelerationStructureEXT rtxTlas;

// EVIL: binding 14 = RTX GI probe irradiance 3D texture (8x8x8 probes x 16x16
// octahedral R11G11B10F). Per Stage 5.5 DDGI consume. Bound to RtxGiProbes.irradianceImage
// when RTX env-gate ON. Type = sampler3D = COMBINED_IMAGE_SAMPLER on C++ side
// (VUID-layout-07990). Layout matches the RtxGiProbes::AllocateTextures VK_IMAGE_TYPE_3D
// VK_FORMAT_B10G11R11_UFLOAT_PACK32 with extent.{w=octSize, h=octSize, d=N^3}.
layout(set = 0, binding = 14) uniform sampler3D rtxGiIrradiance;
// EVIL: binding 15 = RTX GI probe distance 3D texture (8x8x8 probes x 16x16 RG16F).
// Per Stage 5.5 DDGI back-face visibility. Bound to RtxGiProbes.distanceImage when
// RTX env-gate ON. Format = R16G16_SFLOAT matches RtxGiProbes::AllocateTextures.
layout(set = 0, binding = 15) uniform sampler3D rtxGiDistance;
// EVIL: binding 16 = RTX GI probe data 2D texture (1x1 RGBA16F fallback). Per Stage
// 5.5 DDGI probe classification. Bound to RtxGiProbes.probeDataImage when RTX env-gate ON.
layout(set = 0, binding = 16) uniform sampler2D rtxGiProbeData;
// EVIL: binding 17 = RTX GI volume descriptor SSBO. Per Stage 5.5 DDGI consume via
// DDGIGetVolumeIrradiance world position -> probe grid lookup. Bound to
// RtxGiProbes.volumeDescBuffer when RTX env-gate ON. std430 layout matches
// RtxGiProbes::VolumeDescGpu struct (64 bytes).
layout(set = 0, binding = 17, std430) readonly buffer RtxGiVolumeDesc {
    vec4 originAndHalfExtent;
    vec4 invProbeCountAndSpacing;
    vec4 maxRayDistanceAndCounts;
    vec4 pad;
} rtxGiVolume;

// EVIL: binding 18 = voxel-aware RTX shadow mask (R8_UNORM). Per Stage 5.2.E the
// AABB BLAS ray-query path is replaced by an RT pipeline + procedural intersection
// shader that performs DDA over PackedChunkVoxelPayload and writes a per-pixel
// shadow factor (0.0 = in shadow, 1.0 = lit) into this mask. The mask is bound
// from RayTracedShadows::m_shadowMaskImageView when voxel-aware path is active,
// otherwise from a 1x1 R8 fallback image so the slot is always valid for shaders
// compiled with VOXEL_RTX_ENABLED. UV: gl_FragCoord.xy / imageSize(rtxShadowMask).
layout(set = 0, binding = 18) uniform sampler2D rtxShadowMask;

vec3 TraceRtxSmoothSpecularRay(const vec3 worldOrigin, const vec3 reflectionDir, const float maxDistance) {
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, rtxTlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFFu,
        worldOrigin, 0.001, reflectionDir, maxDistance);
    rayQueryProceedEXT(rq);
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        return vec3(0.0);
    }
    return sceneLighting.skyColorAndFogDensity.rgb * sceneLighting.postProcess.y;
}

// EVIL: kRtxSunShadowMaxDistanceMeters removed (session 22x). The old inline ray-query
// path for sun shadows is replaced by an RT pipeline + procedural intersection shader
// (voxel_rtx_shadow.{rgen,rint,rchit,rmiss}) that writes a per-pixel shadow factor into
// a dedicated R8_UNORM mask sampled by ComputeSunShadowSample. The shader-side ray
// query logic that previously generated bounding-box-shaped shadows has been removed.

// EVIL: kRtxAoMinRayLengthMeters = 0.001. T_min offset along ray direction (rayOrigin
// already nudged by normal*0.14 in caller) to avoid self-hit against the surface we
// launched from. Per Khronos VK_KHR_ray_query tutorial: floating-point precision can
// cause the ray to hit its own starting triangle without offset.
const float kRtxAoMinRayLengthMeters = 0.001;

// Returns 1.0 if the AO ray escapes within `radius` meters (visible sky), 0.0 if it
// hits any geometry (occluded). Binary visibility test against scene TLAS; AO strength
// modulation is handled by caller via strength weight, just like the DDA path.
float TraceRtxAmbientOcclusionRay(const vec3 worldOrigin, const vec3 direction, const float radius) {
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, rtxTlas,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 0xFFu,
        worldOrigin, kRtxAoMinRayLengthMeters, direction, radius);
    rayQueryProceedEXT(rq);
    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        return 1.0;
    }
    return 0.0;
}

// Stage 5.5 DDGI consume: trilinear sample of the probe volume at the world position.
// Per docs/experiments/experiments/2026-06-22-ddgi-probe-field-voxel-gi/RESULTS.md the
// 8x8x8=512 probe field with 16x16 octahedral irradiance matches VCT (32.4 dB PSNR
// baseline) at 0.5ms total (1/64 frame round-robin). The full RtxGI-DDGI integration
// is out of scope for session 20x; this stub implements the trilinear fetch + distance
// back-face check that the shader consume needs. The 1/64 frame probe update is a no-op
// in session 20x (handled by Stage 5.5+ follow-up; see TODO.md §5.5).
vec3 SampleRtxGiProbeIrradiance(const vec3 worldPosition, const vec3 normal) {
    const vec3 origin = rtxGiVolume.originAndHalfExtent.xyz;
    const float halfExtent = rtxGiVolume.originAndHalfExtent.w;
    const float invProbeCountX = rtxGiVolume.invProbeCountAndSpacing.x;
    const float invProbeCountY = rtxGiVolume.invProbeCountAndSpacing.y;
    const float invProbeCountZ = rtxGiVolume.invProbeCountAndSpacing.z;
    const uint probeCountX = uint(rtxGiVolume.maxRayDistanceAndCounts.y);
    const uint probeCountY = uint(rtxGiVolume.maxRayDistanceAndCounts.z);
    const uint probeCountZ = uint(rtxGiVolume.maxRayDistanceAndCounts.w);
    if (probeCountX == 0u || probeCountY == 0u || probeCountZ == 0u) {
        return vec3(0.0);
    }
    const vec3 local = clamp((worldPosition - origin) / (2.0 * halfExtent), vec3(0.0), vec3(1.0));
    const vec3 probeFloat = local * vec3(float(probeCountX - 1u), float(probeCountY - 1u), float(probeCountZ - 1u));
    return textureLod(rtxGiIrradiance, (probeFloat + 0.5) * vec3(invProbeCountX, invProbeCountY, invProbeCountZ), 0.0).rgb;
}
#endif

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


layout(location = 2) out vec4 outLayerMask;

layout(location = 3) out vec2 outMotionVector;

const uint kSunContactShadowMaxSteps = 12u;
const uint kAmbientOcclusionMaxSteps = 4u;
const uint kLocalPointLightShadowMaxSteps = 12u;
const float kHugeRayT = 1e20;
const float kVctCutoffRoughness = 0.3f;
const float kVctMaxDistanceMeters = 64.0f;
const uint kVctMaxMipLevel = 4u;

// EVIL: kVctConeDirectionCount=6. Matches TODO.md §5.1 explicit "6 широких конусов"
// diffuse cone tracing. Cones are aligned to the world axes with small upward bias to
// avoid singularity at the floor (Y=0); for full 16/32-cone production quality upgrade
// see WickedEngine VXGI (turanszkij) cone tables.
const uint kVctConeDirectionCount = 6u;
const vec3 kVctConeDirections[6] = vec3[6](
    vec3(1.0, 0.1, 0.0),
    vec3(-1.0, 0.1, 0.0),
    vec3(0.0, 0.1, 1.0),
    vec3(0.0, 0.1, -1.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -0.2, 0.0));

vec3 VctSampleDirectionalCone(
    const vec3 worldOrigin,
    const vec3 coneDir,
    const float coneHalfApertureTan,
    const float maxDistance,
    const uint maxMipLevel) {
    const float maxT = min(maxDistance, kVctMaxDistanceMeters);
    const float mipLevel = clamp(log2(maxT) * 0.5, 0.0, float(maxMipLevel));
    const float blendFalloff = clamp(coneHalfApertureTan, 0.05, 0.6);
    const vec3 nDir = normalize(coneDir);

    vec3 accum = vec3(0.0);
    float weight = 0.0;
    for (int i = 0; i < 3; ++i) {
        const float t = maxT * (float(i) + 0.5) / 3.0;
        const vec3 samplePos = worldOrigin + nDir * t;
        const vec3 clipUvw = samplePos / float(kVctMaxDistanceMeters * 2) + 0.5;
        if (any(lessThan(clipUvw, vec3(0.0))) || any(greaterThan(clipUvw, vec3(1.0)))) {
            continue;
        }
        const float w = 1.0 / (1.0 + blendFalloff * float(i));
        accum += textureLod(vctClipmap, clipUvw, mipLevel).rgb * w;
        weight += w;
    }
    return weight > 0.0 ? accum / weight : vec3(0.0);
}

vec3 VctSampleReflectionCone(
    const vec3 worldOrigin,
    const vec3 viewDirection,
    const vec3 normal,
    const float roughness,
    const float maxDistance,
    const uint maxMipLevel) {
    const vec3 reflectionDir = reflect(-viewDirection, normal);
    const float coneAperture = clamp(roughness * 0.6, 0.05, 0.6);
    return VctSampleDirectionalCone(
        worldOrigin,
        reflectionDir,
        coneAperture,
        maxDistance,
        maxMipLevel);
}

#define DDA_BODY(MAX_STEPS, TRAVELED_OP, PRED, RETURN_EXPR, DEFAULT_RETURN) \
    for (uint stepIndex = 0u; stepIndex < (MAX_STEPS); ++stepIndex) { \
        if (tMax.x < tMax.y) { \
            if (tMax.x < tMax.z) { \
                traveled = tMax.x; \
                currentVoxel.x += stepDirection.x; \
                tMax.x += tDelta.x; \
            } else { \
                traveled = tMax.z; \
                currentVoxel.z += stepDirection.z; \
                tMax.z += tDelta.z; \
            } \
        } else if (tMax.y < tMax.z) { \
            traveled = tMax.y; \
            currentVoxel.y += stepDirection.y; \
            tMax.y += tDelta.y; \
        } else { \
            traveled = tMax.z; \
            currentVoxel.z += stepDirection.z; \
            tMax.z += tDelta.z; \
        } \
        if (traveled TRAVELED_OP maxDistance) { \
            break; \
        } \
        const uint hitMaterial = ReadVoxelMaterial(currentVoxel); \
        if ((PRED)(hitMaterial)) { \
            return (RETURN_EXPR); \
        } \
    } \
    return (DEFAULT_RETURN);

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

    DDA_BODY(maxSteps, >=, IsLocalPointLightShadowOccluder, 1.0 - shadowStrength, 1.0)
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

    DDA_BODY(kSunContactShadowMaxSteps, >, IsSunContactShadowOccluder, 1.0 - contactStrength * (1.0 - clamp(traveled / maxDistance, 0.0, 1.0)), 1.0)
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

    DDA_BODY(maxSteps, >, IsAmbientOcclusionOccluder, (1.0 - clamp(traveled / maxDistance, 0.0, 1.0)) * (1.0 - clamp(traveled / maxDistance, 0.0, 1.0)), 0.0)
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
#ifdef VOXEL_RTX_ENABLED
    if (radius > 0.0) {
        const float rtxVisibility = TraceRtxAmbientOcclusionRay(rayOrigin, rayDirection, radius);
        if (maxSteps == 0u) {
            return 0.0;
        }
        return rtxVisibility;
    }
#endif
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

vec2 SampleSunShadowCascade(
const uint cascadeIndex,
const vec3 receiverPosition,
const float receiverDepthBias,
const float filterRadius,
const float shadowStrength) {
    // CSM removed per TODO.md §5.2.D (session 20x). RTX shadows are the
    // canonical sun shadow path; this helper is retained as a no-op so
    // non-RTX compile variants compile cleanly.
    if (cascadeIndex == 0u) {}
    if (receiverPosition.x + receiverPosition.y + receiverPosition.z == 0.0) {}
    if (receiverDepthBias == 0.0) {}
    if (filterRadius == 0.0) {}
    if (shadowStrength == 0.0) {}
    return vec2(1.0, 0.0);
}

vec4 ComputeSunShadowSample(const vec3 worldPosition, const vec3 normal) {
#ifdef VOXEL_RTX_ENABLED
    {
        const vec3 sunDirForRtx = normalize(sceneLighting.sunDirectionAndWrap.xyz);
        const float nDotLRtx = clamp(dot(normal, sunDirForRtx), 0.0, 1.0);
        if (nDotLRtx > 0.02) {
            const vec2 shadowUv = gl_FragCoord.xy / vec2(textureSize(rtxShadowMask, 0));
            const float rtxLit = texture(rtxShadowMask, shadowUv).r;
            // EVIL: shadowStrength = 0.75. Prevents pitch-black shadows by scaling rtxLit visibility.
            const float blendedRtxLit = mix(0.25, 1.0, rtxLit);
            const float anyRtxShadow = rtxLit < 0.999 ? 1.0 : 0.0;
            return vec4(blendedRtxLit, anyRtxShadow, 0.0, 1.0);
        }
    }
#endif
    const vec3 sunDirFallback = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const float nDotLFallback = clamp(dot(normal, sunDirFallback), 0.0, 1.0);
    if (nDotLFallback <= 0.02) {
        return vec4(1.0, 0.0, 0.0, 1.0);
    }
    return vec4(1.0, 0.0, 0.0, ComputeSunContactVisibility(worldPosition, normal, sunDirFallback));
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
    const float geometryAmbientVisibility = clamp(inAmbientVisibility, 0.0, 1.0);
    const float localAmbientOcclusionVisibility = ComputeAmbientOcclusionVisibility(inWorldPosition, normal);
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

    const float rawLocalPointLightVisibility = ComputeLocalPointLightVisibility(inWorldPosition, normal);

    const float blendedLocalAmbientOcclusionVisibility = mix(localAmbientOcclusionVisibility, historyLayerSample.y, layerBlend);

    const float blendedSunContactVisibility = mix(sunContactVisibility, historyLayerSample.x, layerBlend);
    const float blendedLocalPointLightVisibility = mix(rawLocalPointLightVisibility, historyLayerSample.z, layerBlend);
    const float ambientVisibility = geometryAmbientVisibility * blendedLocalAmbientOcclusionVisibility;
    const vec3 ambient =
    SampleEnvironmentDiffuse(normal) *
    albedo *
    ambientStrength *
    ambientOcclusion *
    ambientVisibility;

    const bool vctEnabled = sceneLighting.vctParams.w > 0.5;
    const float vctConeApertureTan = clamp(sceneLighting.vctParams.x, 0.05, 0.6);
    const float vctMaxDistance = max(sceneLighting.vctParams.y, 0.0);
    vec3 vctDiffuseIrradiance = vec3(0.0);
    if (vctEnabled) {
        for (uint coneIndex = 0u; coneIndex < kVctConeDirectionCount; ++coneIndex) {
            const vec3 rotated = normalize(
                mat3(pushConstants.viewProjection) * kVctConeDirections[coneIndex]);
            vctDiffuseIrradiance += VctSampleDirectionalCone(
                inWorldPosition,
                rotated,
                vctConeApertureTan,
                vctMaxDistance,
                kVctMaxMipLevel);
        }
        vctDiffuseIrradiance /= float(kVctConeDirectionCount);
    }

    const vec3 vctDiffuse = vctEnabled
        ? vctDiffuseIrradiance * albedo * (1.0 / 3.14159265) * ambientVisibility
        : vec3(0.0);

    vec3 vctSpecular = vec3(0.0);
    if (vctEnabled && roughness > kVctCutoffRoughness) {
        const vec3 reflectionIrradiance = VctSampleReflectionCone(
            inWorldPosition,
            viewDirection,
            normal,
            roughness,
            vctMaxDistance,
            kVctMaxMipLevel);
        const float fresnel = pow(1.0 - nDotV, 5.0);
        vctSpecular = reflectionIrradiance * (0.04 + 0.96 * fresnel) * (1.0 - metallic);
    }

#ifdef VOXEL_RTX_ENABLED
    vec3 rtxSmoothSpecular = vec3(0.0);
    if (roughness <= kVctCutoffRoughness && (1.0 - metallic) > 0.01) {
        const vec3 reflectionDir = reflect(-viewDirection, normal);
        const float smoothSpecMaxDistance = min(vctMaxDistance, kVctMaxDistanceMeters);
        const vec3 rtxHit = TraceRtxSmoothSpecularRay(inWorldPosition, reflectionDir, smoothSpecMaxDistance);
        const float fresnel = pow(1.0 - nDotV, 5.0);
        rtxSmoothSpecular = rtxHit * (0.04 + 0.96 * fresnel) * (1.0 - metallic);
    }
#else
    const vec3 rtxSmoothSpecular = vec3(0.0);
#endif

    // EVIL: shadow attenuation for the full compositing pipeline (not just direct sun).
    // directSun already uses shadowedSunColor (= sunColor * sunVisibility), but ambient
    // + AO + contact shadow all still light the surface as if unshadowed, which makes
    // RTX shadows look weak against VoxelLab's checker floor + glass sphere. We want a
    // visible floor shadow under the sphere/columns. pow(sunVisibility, 0.4) is a
    // "soft cap" — full sun (visibility=1) leaves ambient unchanged, while shadowed
    // surface (visibility=0) keeps 0% of its unshadowed ambient contribution. Then we
    // remap so the shadowed surface still has SOME ambient (5%) to avoid a pitch-black
    // look that would lose surface detail under the sphere.
    // EVIL: opacity-aware shadow attenuation. Transparent media (water/glass)
// already receive their own bright contribution from medium transmission
// (line below in main()), so modulating their ambient by shadow would just
// make them darker without a visible shadow on the surface. For purely
// opaque materials we attenuate ambient by the sun shadow ray; for transparent
// media we leave it at 1.0 so their characteristic brightness is preserved
// and the sun shadow does not visually double-darken them.
const float rtxOpaqueShadow = mix(0.15, 1.0, pow(sunVisibility, 0.5));
const float shadowAttenuation = mix(1.0, rtxOpaqueShadow, 1.0 - clamp(material.medium.w, 0.0, 1.0));
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
    vec3 color = (ambient + vctDiffuse + vctSpecular) * shadowAttenuation + directSun + localDirect + grazingTint + rtxSmoothSpecular;

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

    // EVIL: Wronski 2014 froxel screen-space sampling. UVW from gl_FragCoord
    // (normalized to [0, 1] via textureSize) + NDC depth mapped through the
    // exponential depth distribution matching the compute shader's
    // kDepthDistributionGamma=0.5 / kDepthDistributionBias=0.005. This
    // consumes the per-frustum-slab ray-march output written by
    // volumetric_fog.comp. When PROJECTV_FOG=ON not set, binding 12
    // points to a 1x1x1 fallback texture that always returns (0,0,0,0)
    // so the contribute term is a no-op.
    vec3 volumetricFogAccum = vec3(0.0);
    float volumetricFogTransmittance = 1.0;
    {
        const vec3 froxelImageSize = vec3(textureSize(volumetricFog, 0));
        const float nearPlane = max(pushConstants.cameraPosition.w, 0.001);
        const float farPlane = max(pushConstants.cameraForward.w, nearPlane + 0.001);
        const float linearDepth = clamp(viewDistance, nearPlane, farPlane);
        const float normalizedDepth = (linearDepth - nearPlane) / (farPlane - nearPlane);
        const float depthDistribution = pow(normalizedDepth, kFogDepthDistributionExp) * kFogDepthDistributionScale + kFogDepthDistributionBias;
        const vec3 froxelUvw = vec3(
            clamp(gl_FragCoord.x / max(froxelImageSize.x, 1.0), 0.0, 1.0),
            clamp(gl_FragCoord.y / max(froxelImageSize.y, 1.0), 0.0, 1.0),
            clamp(depthDistribution, 0.0, 1.0));
        const vec4 fogSample = texture(volumetricFog, froxelUvw);
        volumetricFogAccum = fogSample.rgb;
        volumetricFogTransmittance = 1.0 - fogSample.a;
    }

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
        OUT_COLOR = vec4(shadowCovered ? mix(vec3(0.65, 0.85, 1.0) * 0.28, vec3(0.65, 0.85, 1.0), sunVisibility) : vec3(1.0, 0.15, 0.10), 1.0);
        return;
    } else if (lightingDebugView == 6u) {
        color = vec3(sunContactVisibility);
    } else if (lightingDebugView == 7u) {
        color = vec3(localAmbientOcclusionVisibility);
    } else if (lightingDebugView == 8u) {
        color = vec3(fog);
    } else if (lightingDebugView == 10u) {
        color = vctDiffuse;
    } else if (lightingDebugView == 11u) {
        color = vctSpecular;
    } else if (lightingDebugView == 12u) {
        color = volumetricFogAccum;
    } else if (lightingDebugView == 13u) {
        color = vec3(volumetricFogTransmittance);
    } else if (lightingDebugView == 14u) {
#ifdef VOXEL_RTX_ENABLED
        color = rtxSmoothSpecular;
#else
        color = vec3(0.0);
#endif
    }

    if (lightingDebugView != 8u) {
        color = mix(color, fogColor, fog);
        color = color * volumetricFogTransmittance + volumetricFogAccum;
    }
    const vec3 linearColor = color;
    color *= max(sceneLighting.postProcess.x, 0.0);
    color = ApplyToneMap(color);
    color = ApplyColorGrading(color);

    outLayerMask = vec4(sunContactVisibility, localAmbientOcclusionVisibility, rawLocalPointLightVisibility, 1.0);

    {
        const vec4 prevClip = sceneLighting.prevViewProjectionMatrix *
            vec4(inWorldPosition, 1.0);
        vec2 motion = vec2(0.0);
        if (prevClip.w > 0.0001) {
            const vec2 prevNdc = prevClip.xy / prevClip.w * 0.5 + 0.5;
            const vec4 currClip = pushConstants.viewProjection *
                vec4(inWorldPosition, 1.0);
            const vec2 currNdc = currClip.xy / currClip.w * 0.5 + 0.5;
            motion = prevNdc - currNdc;
        }
        outMotionVector = motion;
    }

#ifdef TAA_ENABLED
    outSceneColor = vec4(linearColor, material.baseColor.a);
#else
    outColor = vec4(color, material.baseColor.a);
#endif
}
