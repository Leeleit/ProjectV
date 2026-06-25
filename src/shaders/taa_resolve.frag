#version 460



layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D historyColor;
layout(set = 0, binding = 2) uniform sampler2D depth;
layout(set = 0, binding = 4) uniform sampler2D motionVector;

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
} sceneLighting;

layout(push_constant) uniform ResolvePushConstants {
    mat4 inverseCurrentViewProjection;
    mat4 currentViewProjection;
    vec2 renderExtentInverse;

    float taaBlend;
    float taaCasSharpnessMax;
} pushConstants;

layout(location = 0) out vec4 outColor;

const float kHugeTaaRayT = 1e20;
const float kTaaColorDistanceRejectionThreshold = 0.60; // relaxed from 0.40 — accept more history for stronger TAA, fewer visible sub-pixel outliers

// Tonemap and color grading moved to voxel.frag / model.frag so the scene color and
// history are both LDR (post-tonemap+grading). The resolve is now a pure LDR blend;
// the previous HDR current + LDR history mix was undefined and produced outlines.

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


void GetSceneColorRange(
    const vec2 uv,
    const vec2 texelSize,
    out vec3 minColor,
    out vec3 maxColor,
    out vec3 centroidColor,
    out vec3 rgbMin,
    out vec3 rgbMax,
    out vec3 rgbCornerSum)
{
    // Outlier rejection radius: drives min/max/centroid for YCoCg clamp.
    const int radius = clamp(int(sceneLighting.taaHistoryParams.w + 0.5), 1, 7);
    const int snappedRadius = (radius >= 7) ? 7 : (radius >= 5) ? 5 : (radius >= 3) ? 3 : 1;
    const int sideLength = 2 * snappedRadius + 1;
    const float normalizer = 1.0 / float(sideLength * sideLength);
    minColor = vec3(kHugeTaaRayT);
    maxColor = vec3(-kHugeTaaRayT);
    rgbMin = vec3(kHugeTaaRayT);
    rgbMax = vec3(-kHugeTaaRayT);
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

            const bool isCross = (offsetX == 0 || offsetY == 0);
            if (isCross) {
                rgbMin = min(rgbMin, sampleColor);
                rgbMax = max(rgbMax, sampleColor);
            }
        }
    }
    centroidColor = sumColor * normalizer;

    // CAS corner samples: fixed 3x3 window (corners at ±1 texel). Independent of the
    // outlier-rejection radius above because at large radius (e.g. 3 = 7x7) the corners
    // span 6 texels across and the high-pass `center - cornerAvg` over-shoots edges,
    // producing halos around contrast boundaries.
    rgbCornerSum = vec3(0.0);
    for (int offsetY = -1; offsetY <= 1; offsetY += 2) {
        for (int offsetX = -1; offsetX <= 1; offsetX += 2) {
            const vec2 sampleUv = clamp(
                uv + vec2(float(offsetX), float(offsetY)) * texelSize,
                vec2(0.0),
                vec2(1.0));
            rgbCornerSum += texture(sceneColor, sampleUv).rgb;
        }
    }
}


vec3 ApplyCasLinear(
    const vec3 color,
    const vec3 minColor,
    const vec3 maxColor,
    const vec3 cornerSum,
    const float sharpenAmount)
{
    if (sharpenAmount <= 0.0) {
        return color;
    }
    const vec3 cornerAvg = cornerSum * 0.25;
    const vec3 highPass = color - cornerAvg;
    const vec3 range = max(maxColor - minColor, vec3(1e-5));
    const vec3 weight = clamp(highPass / range, vec3(0.0), vec3(1.0));
    return clamp(color + highPass * (sharpenAmount * weight), minColor, maxColor);
}

void main()
{

    const vec2 uv = gl_FragCoord.xy * pushConstants.renderExtentInverse;

    const vec4 currentSample = texture(sceneColor, uv);
    const vec2 texelSize = sceneLighting.taaHistoryParams.xy;
    const bool historyValid = sceneLighting.taaHistoryParams.z > 0.5;

    vec3 historySample = vec3(0.0);
    bool reprojectionOk = false;
    if (historyValid && texelSize.x > 0.0 && texelSize.y > 0.0) {
        const vec2 motion = texture(motionVector, uv).xy;
        const vec2 prevUv = uv + motion;
        if (all(greaterThanEqual(prevUv, vec2(0.0))) &&
            all(lessThanEqual(prevUv, vec2(1.0)))) {
            historySample = texture(historyColor, prevUv).rgb;
            reprojectionOk = true;
        }
    }

    vec3 minColor;
    vec3 maxColor;
    vec3 centroidColor;
    vec3 rgbMin;
    vec3 rgbMax;
    vec3 rgbCornerSum;
    GetSceneColorRange(uv, texelSize, minColor, maxColor, centroidColor, rgbMin, rgbMax, rgbCornerSum);
    const vec3 currentYCoCg = RGBToYCoCg(texture(sceneColor, uv).rgb);

    const float currentToCentroidDistance = length(currentYCoCg - centroidColor);
    const bool isOutlier = currentToCentroidDistance > kTaaColorDistanceRejectionThreshold;

    const vec3 clampedCurrent = isOutlier
        ? YCoCgToRGB(currentYCoCg)
        : YCoCgToRGB(clamp(currentYCoCg, minColor, maxColor));

    const float temporalBlend = historyValid && reprojectionOk
        ? clamp(sceneLighting.taaParams.z, 0.0, 1.0)
        : 0.0;

    const float blendFactor = isOutlier ? 0.0 : temporalBlend;


    const vec3 clampedHistory = isOutlier
        ? YCoCgToRGB(RGBToYCoCg(historySample))
        : YCoCgToRGB(clamp(RGBToYCoCg(historySample), minColor, maxColor));


    vec3 resolvedColor = mix(clampedCurrent, clampedHistory, blendFactor);

    // CAS sharpening scales with taaBlend: higher blend = stronger temporal accumulation = more blur = more sharpening needed to recover detail.
    // Previous formula `(1.0 - blend) * max` was inverted — high blend gave less sharpening, which made temporal blur visible.
    const float sharpenAmount = clamp(pushConstants.taaBlend, 0.0, 1.0) * pushConstants.taaCasSharpnessMax;
    resolvedColor = ApplyCasLinear(resolvedColor, rgbMin, rgbMax, rgbCornerSum, sharpenAmount);

    outColor = vec4(resolvedColor, 1.0);
}
