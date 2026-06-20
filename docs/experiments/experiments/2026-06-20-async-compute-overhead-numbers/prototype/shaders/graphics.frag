#version 450

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
    uint pad0;
    uint pad1;
} pc;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0);
    vec3 col = vec3(uv, 0.5 + 0.5 * sin(pc.time));
    fragColor = vec4(col, 1.0);
}