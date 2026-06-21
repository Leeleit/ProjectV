#version 450

// Pipeline A fragment shader: simple Lambert lighting + write motion vector MRT.
// Motion vector = (curr_clip - prev_clip) / curr_clip.w  (perspective-correct).

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec4 vCurrClip;
layout(location = 3) in vec4 vPrevClip;

layout(set = 0, binding = 0) uniform UboA {
    mat4 viewProjCurr;
    mat4 viewProjPrev;
    vec3 camPosCurr;
    float time;
} ubo;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outMotion;  // R16G16_SFLOAT MRT

layout(push_constant) uniform Push {
    vec3 lightDir;
    float _pad0;
    vec3 objectColor;
    float _pad1;
} pc;

void main() {
    // Simple Lambert lighting
    float ndotl = max(dot(normalize(pc.lightDir), vec3(0.0, 1.0, 0.0)), 0.0);
    float shade = 0.4 + 0.6 * ndotl;
    outColor = vec4(pc.objectColor * shade, 1.0);

    // Perspective-correct motion vector (per Karis 2014 "Brute Force" + R16G16_SFLOAT)
    vec2 currScreen = vCurrClip.xy / vCurrClip.w;
    vec2 prevScreen = vPrevClip.xy / vPrevClip.w;
    outMotion = (prevScreen - currScreen) * 0.5 + 0.5;  // map to [0,1] for R16G16_UNORM-like behavior
}
