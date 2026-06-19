// SPDX-License-Identifier: MIT
// lighting.glsl — shared PBR direct-light math for the voxel pass
// (`voxel.frag`) and the new model pass (`model.frag`). The actual
// SceneLightingBuffer struct declaration stays per-shader for now
// because `voxel.frag` and `voxel_shadow.vert` and `voxel_mesh.comp`
// all carry their own byte-identical copy of the buffer (see
// `agent/decisions.md` §18); pulling that declaration out of the
// three `.frag`/`.vert`/`.comp` files is a separate refactor and
// not in M4's scope. This file is just the math.

// Schlick-GGX distribution, geometry, Smith, and Fresnel-Schlick
// are the canonical direct-light PBR building blocks, lifted from
// `voxel.frag` (lines 575-594 in the pre-M4 mainline) so the model
// pass evaluates the same BRDF as the voxel pass.
float ProjectV_DistributionGGX(const float nDotH, const float roughness) {
    const float alpha = roughness * roughness;
    const float alphaSq = alpha * alpha;
    const float denom = max(nDotH * nDotH * (alphaSq - 1.0) + 1.0, 0.0001);
    return alphaSq / (3.14159265 * denom * denom);
}

float ProjectV_GeometrySchlickGGX(const float nDotX, const float roughness) {
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0;
    return nDotX / max(nDotX * (1.0 - k) + k, 0.0001);
}

float ProjectV_GeometrySmith(const float nDotV, const float nDotL, const float roughness) {
    return ProjectV_GeometrySchlickGGX(nDotV, roughness) * ProjectV_GeometrySchlickGGX(nDotL, roughness);
}

vec3 ProjectV_FresnelSchlick(const float cosTheta, const vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
}

/// \brief Wrapped-diffuse + GGX direct-light evaluation.
///
/// \details
/// Inputs:
///    lightDirection, lightRadiance -- the per-light L vector and

///      incoming radiance (already attenuated for distance / cone

///      falloff in the caller);

///    normal, viewDirection -- world-space N and V;

///    albedo, roughness, metallic, reflectance -- PBR surface;

///    directDiffuseStrength -- authored diffuse response weight;

///    diffuseWrap -- authored wrap parameter (0 = Lambert, >0 softens

///      the terminator).

///  Returns the unshadowed direct contribution. Shadow visibility is

///  the caller's responsibility (voxel pass adds the CSM visibility

///  term; the M4 model pass is unshadowed direct light only).

vec3 ProjectV_EvaluateDirectLighting(
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
    const vec3 fresnel = ProjectV_FresnelSchlick(hDotV, f0);
    const float distribution = ProjectV_DistributionGGX(nDotH, roughness);
    const float geometry = ProjectV_GeometrySmith(nDotV, nDotL, roughness);
    const vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotL * nDotV, 0.0001);
    const vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo;
    return diffuse * lightRadiance * wrappedDiffuse * directDiffuseStrength + specular * lightRadiance * nDotL;
}

vec3 ProjectV_SampleEnvironmentDiffuse(
    const vec3 normal,
    const vec3 skyColor,
    const vec3 horizonColor,
    const vec3 groundColor,
    const float environmentIntensity) {
    const float up = clamp(normal.y, -1.0, 1.0);
    const float skyWeight = smoothstep(-0.25, 0.85, up);
    const float groundWeight = smoothstep(0.35, -0.75, up) * 0.65;
    const float horizonWeight = pow(1.0 - abs(up), 2.0) * 0.55;
    const vec3 environment = skyColor * skyWeight + groundColor * groundWeight + horizonColor * (horizonWeight + 0.08);
    return environment * max(environmentIntensity, 0.0);
}

vec3 ProjectV_ApplyToneMap(const vec3 linearColor, const uint toneMapOperator) {
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

vec3 ProjectV_ApplyColorGrading(
    const vec3 mappedColor,
    const float whitePoint,
    const float contrast,
    const float saturation,
    const float lift) {
    const vec3 normalizedColor = mappedColor / whitePoint;
    const float luma = dot(normalizedColor, vec3(0.2126, 0.7152, 0.0722));
    const vec3 saturatedColor = mix(vec3(luma), normalizedColor, saturation);
    return clamp((saturatedColor - vec3(0.5)) * contrast + vec3(0.5 + lift), 0.0, 1.0);
}
