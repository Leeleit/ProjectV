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
const float kTaaColorDistanceRejectionThreshold = 0.40;

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
    const int radius = clamp(int(sceneLighting.taaHistoryParams.w + 0.5), 1, 7);

    const int snappedRadius = (radius >= 7) ? 7 : (radius >= 5) ? 5 : (radius >= 3) ? 3 : 1;
    const int sideLength = 2 * snappedRadius + 1;
    const float normalizer = 1.0 / float(sideLength * sideLength);
    minColor = vec3(kHugeTaaRayT);
    maxColor = vec3(-kHugeTaaRayT);
    rgbMin = vec3(kHugeTaaRayT);
    rgbMax = vec3(-kHugeTaaRayT);
    rgbCornerSum = vec3(0.0);
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

            const bool isCorner = (offsetX == -snappedRadius || offsetX == snappedRadius) &&
                                  (offsetY == -snappedRadius || offsetY == snappedRadius);
            const bool isCross = (offsetX == 0 || offsetY == 0);
            if (isCorner) {
                rgbCornerSum += sampleColor;
            }
            if (isCross) {
                rgbMin = min(rgbMin, sampleColor);
                rgbMax = max(rgbMax, sampleColor);
            }
        }
    }
    centroidColor = sumColor * normalizer;
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


    vec3 linearOut = mix(clampedCurrent, clampedHistory, blendFactor);

    linearOut *= max(sceneLighting.postProcess.x, 0.0);


    const float sharpenAmount = max(0.0, (1.0 - pushConstants.taaBlend) * pushConstants.taaCasSharpnessMax);
    linearOut = ApplyCasLinear(linearOut, rgbMin, rgbMax, rgbCornerSum, sharpenAmount);

    vec3 mappedOut = ApplyTaaToneMap(linearOut);
    mappedOut = ApplyTaaColorGrading(mappedOut);
    outColor = vec4(mappedOut, 1.0);
}
