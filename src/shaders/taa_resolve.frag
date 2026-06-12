#version 460

// TAA resolve pass. Reads the current jittered scene color and the previous
// frame's resolved history, reprojects history through `prevViewProjection` +
// `inverse(currViewProjection)`, clamps the history against a 3x3 YCoCg
// neighbourhood of the current sample, and blends with the configurable
// `taaParams.z` factor. Applies tone-map and color grading here so the
// main voxel pass can stay in linear light. Outputs the final sRGB-encoded
// color straight to the swapchain.
//
// YCoCg clamp rationale (vs RGB clamp): on bright highlights the RGB
// neighbourhood can have a huge R range, which under RGB clamp either
// discards the highlight or lets a single hot neighbour pull the median
// too high. YCoCg separates luma (Y) from chroma (Co, Cg), so a 1-tap
// bright pixel affects only Y; the chroma component still preserves the
// per-sample tint, and the history clamp doesn't wash coloured highlights
// toward grey. Reference: Yang, "Improved YCoCg Neighborhood Clamp for
// Temporal Anti-Aliasing", GPU Gems 3 / MJP notes.

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
// Color-distance rejection threshold in YCoCg space. When the current
// sample is farther than this from the neighborhood centroid, the
// shader assumes the surrounding samples are from a different
// material/surface (e.g. a polygon-model pass output surrounded by
// voxel pass samples — see M5.2 follow-up from the asset-pipeline
// closeout) and skips both the YCoCg clamp and the temporal blend.
// 0.20 covers a saturated channel in [0, 1] linear without false
// positives on natural voxel-pass variation.
const float kTaaColorDistanceRejectionThreshold = 0.20;

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

// YCoCg is a lossless reversible color space derived from RGB. The exact
// (non-approximate) transform below keeps full float precision; clamping in
// YCoCg space and converting back round-trips RGB exactly. Co and Cg span
// roughly [-1, 1] for in-gamut colours, Y spans [0, 1] (matching RGB here
// because inputs are in [0, 1] linear light).
vec3 RGBToYCoCg(const vec3 rgb) {
    const float co = rgb.r - rgb.b;
    const float tmp = rgb.b + co * 0.5;
    const float cg = rgb.g - tmp;
    const float y = tmp + cg * 0.5;
    return vec3(y, co, cg);
}

vec3 YCoCgToRGB(const vec3 ycocg) {
    const float tmp = ycocg.x - ycocg.z * 0.5;
    const float g = tmp + ycocg.z;
    const float b = tmp - ycocg.y * 0.5;
    const float r = b + ycocg.y;
    return vec3(r, g, b);
}

void GetSceneColorRange(const vec2 uv, const vec2 texelSize, out vec3 minColor, out vec3 maxColor, out vec3 centroidColor) {
    // 3x3 / 5x5 / 7x7 min/max in YCoCg space, returned as YCoCg (Y, Co, Cg).
    // The per-axis radius comes from `sceneLighting.taaHistoryParams.w`
    // (set by `RenderState::taaNeighbourhoodRadius`, live `,` key in
    // `InputAction::CycleTaaNeighbourhoodRadius`). Allowed values are
    // 1, 3, 5, 7; the loop is clamped to the same set so a stale or
    // out-of-range value won't drive the loop into undefined territory.
    // The caller is expected to clamp a sample in YCoCg space and convert
    // back to RGB. The third out-param `centroidColor` is the arithmetic
    // mean of the same neighbourhood in YCoCg space; `main()` uses it
    // for the M5.2 color-distance rejection (skip clamp + history blend
    // when current is far from this mean — see comment above
    // `kTaaColorDistanceRejectionThreshold`).
    const int radius = clamp(int(sceneLighting.taaHistoryParams.w + 0.5), 1, 7);
    // Snap odd-only: 1, 3, 5, 7. Anything in between behaves like the
    // lower bound (1, 3 or 5), so the cycle is well-defined.
    const int snappedRadius = (radius >= 7) ? 7 : (radius >= 5) ? 5 : (radius >= 3) ? 3 : 1;
    const int sideLength = 2 * snappedRadius + 1;
    const float normalizer = 1.0 / float(sideLength * sideLength);
    minColor = vec3(kHugeTaaRayT);
    maxColor = vec3(-kHugeTaaRayT);
    vec3 sumColor = vec3(0.0);
    for (int offsetY = -snappedRadius; offsetY <= snappedRadius; ++offsetY) {
        for (int offsetX = -snappedRadius; offsetX <= snappedRadius; ++offsetX) {
            const vec2 sampleUv = clamp(
                uv + vec2(float(offsetX), float(offsetY)) * texelSize,
                vec2(0.0),
                vec2(1.0));
            const vec3 sampleColor = texture(sceneColor, sampleUv).rgb;
            const vec3 sampleYCoCg = RGBToYCoCg(sampleColor);
            minColor = min(minColor, sampleYCoCg);
            maxColor = max(maxColor, sampleYCoCg);
            sumColor += sampleYCoCg;
        }
    }
    centroidColor = sumColor * normalizer;
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

    vec3 minColor;
    vec3 maxColor;
    vec3 centroidColor;
    GetSceneColorRange(uv, texelSize, minColor, maxColor, centroidColor);
    // `minColor` / `maxColor` / `centroidColor` are in YCoCg space (Y, Co, Cg).
    const vec3 currentYCoCg = RGBToYCoCg(texture(sceneColor, uv).rgb);
    // M5.2 color-distance rejection: when the current sample is far from
    // the neighbourhood mean, both the YCoCg clamp and the temporal blend
    // are skipped. The clamp alone is not enough — without this guard,
    // a polygon-model pass pixel surrounded by voxel pass samples gets
    // pulled into the voxel range (since the model often has a
    // saturated/distinct colour that the voxel range doesn't include).
    // The history blend is also skipped because the previous frame's
    // history at the same UV is most likely *also* a model pixel that
    // already got collapsed, so blending with it would re-introduce
    // the bug. Source of the original report: the asset-pipeline
    // session closeout (`b152b70`) M5.2 follow-up; fix landed in
    // `taa_resolve.frag` per AGENTS.md §7.2.6 (TAA-scope).
    const float currentToCentroidDistance = length(currentYCoCg - centroidColor);
    const bool isOutlier = currentToCentroidDistance > kTaaColorDistanceRejectionThreshold;
    // Clamp the current sample in YCoCg space and convert back to RGB so
    // the result can blend with the (also-RGB) history sample.
    const vec3 clampedCurrent = isOutlier
        ? YCoCgToRGB(currentYCoCg)
        : YCoCgToRGB(clamp(currentYCoCg, minColor, maxColor));

    const float temporalBlend = historyValid && reprojectionOk
        ? clamp(sceneLighting.taaParams.z, 0.0, 1.0)
        : 0.0;
    // Outliers skip the temporal blend: the history sample at the same
    // UV is most likely a previous-frame model pixel that already got
    // clamped into the voxel range, so blending with it would smear the
    // collapse. The current sample is used as-is, no history.
    const float blendFactor = isOutlier ? 0.0 : temporalBlend;

    // Clamp the history sample against the current 3x3 YCoCg neighborhood
    // so that wrong reprojections (revealed geometry after camera movement)
    // do not ghost a completely different surface into the blend. Same
    // YCoCg-then-back-to-RGB flow as `clampedCurrent`. Outliers skip the
    // history entirely (blendFactor=0 above), but we still pass through
    // the same RGB conversion so the math stays consistent.
    const vec3 clampedHistory = isOutlier
        ? YCoCgToRGB(RGBToYCoCg(historySample))
        : YCoCgToRGB(clamp(RGBToYCoCg(historySample), minColor, maxColor));

    // Linear history: history was stored *before* tone-map was applied so
    // that successive blends happen in linear light. The resolve passes
    // the linear result through tone-map + grading here, so the swapchain
    // output remains bit-stable across the TAA path.
    vec3 linearOut = mix(clampedCurrent, clampedHistory, blendFactor);
    // Apply exposure to the current sample as well so the in/out color
    // spaces stay consistent with the main pass.
    linearOut *= max(sceneLighting.postProcess.x, 0.0);

    vec3 mappedOut = ApplyTaaToneMap(linearOut);
    mappedOut = ApplyTaaColorGrading(mappedOut);
    outColor = vec4(mappedOut, 1.0);
}
