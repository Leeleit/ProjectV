#version 450

// Pipeline A vertex shader: writes per-vertex motion vector via MRT.
// TODO.md §5.3 explicit format: VK_FORMAT_R16G16_SFLOAT.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform UboA {
    mat4 viewProjCurr;
    mat4 viewProjPrev;
    vec3 camPosCurr;
    float time;
} ubo;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec4 vCurrClip;  // current clip-space pos (for TAA resolve)
layout(location = 3) out vec4 vPrevClip;  // previous clip-space pos (for TAA resolve motion vector)

void main() {
    vec4 curr = ubo.viewProjCurr * vec4(inPos, 1.0);
    vec4 prev = ubo.viewProjPrev * vec4(inPos, 1.0);
    gl_Position = curr;
    vColor = inColor;
    vUV = inUV;
    vCurrClip = curr;
    vPrevClip = prev;
}
