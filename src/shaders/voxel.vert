#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inMaterialKind;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pushConstants;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outDepth;
layout(location = 3) out float outMaterialKind;

void main() {
    gl_Position = pushConstants.viewProjection * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outColor = inColor;
    outDepth = gl_Position.z / gl_Position.w;
    outMaterialKind = inMaterialKind;
}
