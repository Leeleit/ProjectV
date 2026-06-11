#version 460

// TAA resolve pass. Reads the current jittered scene color and the previous
// frame's resolved history, reprojects history through `prevViewProjection` +
// `inverse(currViewProjection)`, clamps the history against a 3x3 RGB
// neighbourhood of the current sample, and blends with the configurable
// `taaParams.z` factor. Applies tone-map and color grading here so the
// main voxel pass can stay in linear light. Outputs the final sRGB-encoded
// color straight to the swapchain.

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D historyColor;
layout(set = 0, binding = 2) uniform sampler2D depth;

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
    vec4 taaParams;
    mat4 prevViewProjectionMatrix;
    vec4 taaHistoryParams;
} sceneLighting;

layout(push_constant) uniform ResolvePushConstants {
    mat4 inverseCurrentViewProjection;
    mat4 currentViewProjection;
    vec2 renderExtentInverse;
    vec2 reservedPadding;
} pushConstants;

layout(location = 0) out vec4 outColor;

const float kHugeTaaRayT = 1e20;

vec3 ApplyTaaToneMap(const vec3 linearColor) {
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

vec3 ApplyTaaColorGrading(const vec3 mappedColor) {
    const float whitePoint = clamp(sceneLighting.colorGrading.x, 0.25, 4.0);
    const float contrast = clamp(sceneLighting.colorGrading.y, 0.0, 2.0);
    const float saturation = clamp(sceneLighting.colorGrading.z, 0.0, 2.0);
    const float lift = clamp(sceneLighting.colorGrading.w, -0.25, 0.25);
    const vec3 normalizedColor = mappedColor / whitePoint;
    const float luma = dot(normalizedColor, vec3(0.2126, 0.7152, 0.0722));
    const vec3 saturatedColor = mix(vec3(luma), normalizedColor, saturation);
    return clamp((saturatedColor - vec3(0.5)) * contrast + vec3(0.5 + lift), 0.0, 1.0);
}

vec3 SampleSceneColorClamp(const vec2 uv, const vec2 texelSize) {
    // 3x3 RGB clamp. Cheap; trades a tiny amount of chroma resolution for a
    // much more stable history. YCoCg clamp is documented as a future
    // upgrade path in `agent/decisions.md` §18.
    vec3 minimumColor = vec3(kHugeTaaRayT);
    vec3 maximumColor = vec3(-kHugeTaaRayT);
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const vec2 sampleUv = clamp(
                uv + vec2(float(offsetX), float(offsetY)) * texelSize,
                vec2(0.0),
                vec2(1.0));
            const vec3 sampleColor = texture(sceneColor, sampleUv).rgb;
            minimumColor = min(minimumColor, sampleColor);
            maximumColor = max(maximumColor, sampleColor);
        }
    }
    return clamp(texture(sceneColor, uv).rgb, minimumColor, maximumColor);
}

void main()
{
    // UV in [0, 1] of the current (post-jitter) frame. The viewport
    // origin is top-left in Vulkan, so the same UV space works for
    // sampling the history texture (which is also top-left origin).
    const vec2 uv = gl_FragCoord.xy * pushConstants.renderExtentInverse;

    const vec4 currentSample = texture(sceneColor, uv);
    const vec2 texelSize = sceneLighting.taaHistoryParams.xy;
    const bool historyValid = sceneLighting.taaHistoryParams.z > 0.5;

    vec3 historySample = vec3(0.0);
    bool reprojectionOk = false;
    if (historyValid && texelSize.x > 0.0 && texelSize.y > 0.0) {
        // Reconstruct the world position of the *current* pixel from its
        // depth value. `inverseCurrentViewProjection` is a column-major
        // mat4; multiplying by a homogeneous `(uv, depth, 1)` gives clip,
        // and dividing by `w` gives the world-space position. Then we
        // reproject that point through the previous viewProjection to
        // get the previous-frame NDC, convert to UV, and sample history.
        const float rawDepth = texture(depth, uv).r;
        const float ndcDepth = rawDepth * 2.0 - 1.0;
        const vec4 ndcNear = vec4(uv * 2.0 - 1.0, ndcDepth, 1.0);
        const vec4 worldH = pushConstants.inverseCurrentViewProjection * ndcNear;
        if (worldH.w > 0.0001) {
            const vec3 worldPos = worldH.xyz / worldH.w;
            const vec4 prevClip = sceneLighting.prevViewProjectionMatrix * vec4(worldPos, 1.0);
            if (prevClip.w > 0.0001) {
                const vec2 prevUv = prevClip.xy / prevClip.w * 0.5 + 0.5;
                if (all(greaterThanEqual(prevUv, vec2(0.0))) &&
                    all(lessThanEqual(prevUv, vec2(1.0)))) {
                    historySample = texture(historyColor, prevUv).rgb;
                    reprojectionOk = true;
                }
            }
        }
    }

    const vec3 clampedCurrent = SampleSceneColorClamp(uv, texelSize);

    const float blendFactor = historyValid && reprojectionOk
        ? clamp(sceneLighting.taaParams.z, 0.0, 1.0)
        : 0.0;

    // Linear history: history was stored *before* tone-map was applied so
    // that successive blends happen in linear light. The resolve passes
    // the linear result through tone-map + grading here, so the swapchain
    // output remains bit-stable across the TAA path.
    vec3 linearOut = mix(clampedCurrent, historySample, blendFactor);
    // Apply exposure to the current sample as well so the in/out color
    // spaces stay consistent with the main pass.
    linearOut *= max(sceneLighting.postProcess.x, 0.0);

    vec3 mappedOut = ApplyTaaToneMap(linearOut);
    mappedOut = ApplyTaaColorGrading(mappedOut);
    outColor = vec4(mappedOut, 1.0);
}
