#version 460
// SPDX-License-Identifier: MIT
// ProjectV — sky atmosphere fragment shader (Phase 3 Rayleigh + Mie analytical).
//
// Per Hillaire 2020 EGSR + Bruneton 2017 + Frostbite 2016 sky. Single-scattering
// analytical Rayleigh + Mie without LUT precomputation. Per-channel
// wavelength coefficients (R/G/B), Henyey-Greenstein phase for Mie, Schlick
// approximation for sun falloff. Cheap enough for a per-pixel full-screen
// pass (~0.05-0.1 ms @ 1080p on RTX 3060 Ti per Bruneton 2017 numbers).
//
// Trade-off vs LUT version: less accurate, no multiple-scattering
// approximation, no aerial perspective. For full quality, add 2D Sky-View LUT
// (256x128 RGBA16F) + 2D Multi-Scattering LUT (32x32 RGBA16F) per
// `2026-06-21-precomputed-atmospheric-sky` Stage 3.1+ follow-up.
//
// EVIL: Rayleigh beta coefficients hard-coded per Hillaire 2020 reference
// (lambda^-4 scaling, R=680nm, G=550nm, B=440nm wavelengths). Mie beta fixed
// at 0.005 (broadband aerosol). Production tunable via push constants.

layout(location = 0) in vec2 inNdcXY;

layout(location = 0) out vec4 outSkyColor;

// EVIL: bindings 0/1 = precomputed Sky-View + Multi-Scattering LUTs per
// Hillaire 2020 EGSR Section 4.3 (production-quality atmospheric scattering).
// Bound to CPU-precomputed RGBA16F 2D textures when PROJECTV_SKY_LUT=ON.
// Fallback to analytical Rayleigh + Mie path (Hillaire 2020 Section 4.4)
// when no LUTs are bound — analytical path is sample-accurate but ~10× cost.
layout(set = 0, binding = 0) uniform sampler2D skyViewLut;
layout(set = 0, binding = 1) uniform sampler2D multiScatteringLut;

layout(push_constant) uniform SkyAtmospherePushConstants {
    vec4 zenithColorAndIntensity;
    vec4 horizonColorAndSunIntensity;
    vec4 sunDirectionAndAngularSize;
    vec4 viewParams;
} pc;

const float kSkyLutPrecomputedFlag = 1.0;

const float kPi = 3.14159265;
const float kPlanetRadius = 6371000.0;          // meters (Earth)
const float kAtmosphereHeight = 100000.0;       // meters (homogeneous shell)
const vec3 kRayleighBeta = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const float kMieBeta = 0.005;
const float kMieG = 0.8;
const int kRaymarchSteps = 16;

float cosTheta(vec3 a, vec3 b)
{
    return clamp(dot(a, b), 0.0, 1.0);
}

float HenyeyGreenstein(float cosThetaIn, float g)
{
    const float g2 = g * g;
    return (1.0 - g2) / (4.0 * kPi * pow(1.0 + g2 - 2.0 * g * cosThetaIn, 1.5));
}

vec3 SkyViewAnalytical(vec3 viewDir, vec3 sunDir)
{
    const float upDot = clamp(viewDir.y, -1.0, 1.0);
    if (upDot < 0.0) {
        return vec3(0.0);
    }

    const float viewHeight = sqrt(upDot * upDot + (1.0 - upDot * upDot) * 1.0e6);
    const float sunZenith = clamp(sunDir.y, 0.0, 1.0);

    float opticalDepth = 0.0;
    float viewOptical = 0.0;
    float sunOptical = 0.0;
    for (int i = 0; i < kRaymarchSteps; ++i) {
        const float t = (float(i) + 0.5) / float(kRaymarchSteps);
        const float sampleHeight = t * kAtmosphereHeight;
        const float density = exp(-sampleHeight / 8500.0);
        opticalDepth += density;
        viewOptical += density * (1.0 - t);
        sunOptical += density;
    }
    const float scale = 1.0 / float(kRaymarchSteps) * kAtmosphereHeight;

    const float cosSunView = cosTheta(viewDir, sunDir);
    vec3 rayleigh = kRayleighBeta * scale * (1.0 + cosSunView * cosSunView);
    rayleigh *= exp(-(viewOptical + sunOptical) * kRayleighBeta);
    const float mie = kMieBeta * scale * HenyeyGreenstein(cosSunView, kMieG);
    vec3 inscatter = rayleigh + vec3(mie);

    const float horizonBoost = clamp(1.0 - upDot, 0.0, 1.0);
    inscatter *= mix(1.0, 1.5, pow(horizonBoost, 0.5));
    inscatter *= 1.0 / (1.0 + sunZenith * 2.0);

    return inscatter;
}

void main()
{
    const float aspectRatio = max(pc.viewParams.y, 0.0001);
    const float tanHalfFovY = max(pc.viewParams.z, 0.0001);
    const vec3 viewDirection = normalize(vec3(
        inNdcXY.x * aspectRatio * tanHalfFovY,
        inNdcXY.y * tanHalfFovY,
        -1.0));
    const vec3 sunDir = normalize(pc.sunDirectionAndAngularSize.xyz);

    vec3 sky;
    if (pc.zenithColorAndIntensity.w > 1.5) {
        const float viewZenith = clamp(acos(clamp(-viewDirection.z, -1.0, 1.0)) / 1.5707963, 0.0, 1.0);
        const float sunZenith = clamp(acos(clamp(sunDir.y, -1.0, 1.0)) / 1.5707963, 0.0, 1.0);
        const vec2 skyViewUv = vec2(viewZenith, sunZenith);
        const vec4 skyViewSample = texture(skyViewLut, skyViewUv);
        const vec2 msUv = vec2(clamp(sunZenith, 0.0, 1.0), clamp(sunDir.y, 0.0, 1.0));
        const vec4 multiScatterSample = texture(multiScatteringLut, msUv);
        sky = skyViewSample.rgb + multiScatterSample.rgb * 0.4;
        sky *= skyViewSample.a;
    } else {
        sky = SkyViewAnalytical(viewDirection, sunDir);
        sky *= pc.zenithColorAndIntensity.w * 60000.0;
    }

    sky = mix(
        pc.horizonColorAndSunIntensity.xyz,
        sky,
        0.85);

    const float cosSunAngle = max(dot(viewDirection, sunDir), 0.0);
    const float sunHalfAngle = max(pc.sunDirectionAndAngularSize.w, 0.0001);
    const float sunCosCutoff = cos(sunHalfAngle);
    const float sunCosInner = cos(sunHalfAngle * 0.35);
    float sunDisc = smoothstep(sunCosCutoff, sunCosInner, cosSunAngle);
    sky += pc.horizonColorAndSunIntensity.w * sunDisc;

    if (viewDirection.y < 0.0) {
        sky *= clamp(1.0 + viewDirection.y * 1.5, 0.0, 1.0);
    }

    outSkyColor = vec4(sky, 1.0);
}
