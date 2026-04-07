#version 460

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 overlayData0;
    vec4 overlayData1;
    vec4 overlayColor;
} pushConstants;

layout(location = 0) out vec4 outColor;

const vec3 kBoxCorners[8] = vec3[](
vec3(0.0, 0.0, 0.0),
vec3(1.0, 0.0, 0.0),
vec3(1.0, 1.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0),
vec3(1.0, 0.0, 1.0),
vec3(1.0, 1.0, 1.0),
vec3(0.0, 1.0, 1.0)
);

const int kBoxLineVertexIndices[24] = int[](
0, 1, 1, 2, 2, 3, 3, 0,
4, 5, 5, 6, 6, 7, 7, 4,
0, 4, 1, 5, 2, 6, 3, 7
);

vec4 BuildSelectionVertexPosition() {
    const vec3 worldMin = pushConstants.overlayData0.xyz;
    const vec3 worldMax = pushConstants.overlayData1.xyz;
    const int cornerIndex = kBoxLineVertexIndices[gl_VertexIndex];
    const vec3 cornerLerp = kBoxCorners[cornerIndex];
    const vec3 worldPosition = mix(worldMin, worldMax, cornerLerp);
    return pushConstants.viewProjection * vec4(worldPosition, 1.0);
}

vec4 BuildCrosshairVertexPosition() {
    const float halfWidth = pushConstants.overlayData0.x;
    const float halfHeight = pushConstants.overlayData0.y;

    switch (gl_VertexIndex) {
        case 0: return vec4(-halfWidth, 0.0, 0.0, 1.0);
        case 1: return vec4(halfWidth, 0.0, 0.0, 1.0);
        case 2: return vec4(0.0, -halfHeight, 0.0, 1.0);
        default : return vec4(0.0, halfHeight, 0.0, 1.0);
    }
}

void main() {
    outColor = pushConstants.overlayColor;
    if (pushConstants.overlayData0.w < 0.5) {
        gl_Position = BuildSelectionVertexPosition();
    } else {
        gl_Position = BuildCrosshairVertexPosition();
    }
}
