#version 450

// Pipeline B fragment shader: writes color + depth (D32F) for reconstruction in TAA resolve.

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec4 vWorldPos;

layout(set = 0, binding = 0) uniform UboB {
    mat4 viewProjCurr;
    mat4 invViewProjCurr;
} ubo;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    vec3 lightDir;
    float _pad0;
    vec3 objectColor;
    float _pad1;
} pc;

void main() {
    float ndotl = max(dot(normalize(pc.lightDir), vec3(0.0, 1.0, 0.0)), 0.0);
    float shade = 0.4 + 0.6 * ndotl;
    outColor = vec4(pc.objectColor * shade, 1.0);
}
