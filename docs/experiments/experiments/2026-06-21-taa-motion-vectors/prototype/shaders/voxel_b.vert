#version 450

// Pipeline B vertex shader: NO motion vector MRT.
// Motion vector is reconstructed in TAA resolve from depth buffer + prev-viewProj.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform UboB {
    mat4 viewProjCurr;
    mat4 invViewProjCurr;
} ubo;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec4 vWorldPos;

void main() {
    gl_Position = ubo.viewProjCurr * vec4(inPos, 1.0);
    vColor = inColor;
    vUV = inUV;
    vWorldPos = vec4(inPos, 1.0);
}
