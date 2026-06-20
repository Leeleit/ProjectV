#version 460
// Fullscreen triangle vertex shader (resolve pass dispatch).
// Generates a triangle covering the entire viewport via gl_VertexIndex.
vec2 FullscreenUV(uint vid) {
    vec2 uv = vec2((vid << 1) & 2, vid & 2);
    return uv * 2.0 - 1.0;
}
void main() {
    gl_Position = vec4(FullscreenUV(gl_VertexIndex), 0.0, 1.0);
}
