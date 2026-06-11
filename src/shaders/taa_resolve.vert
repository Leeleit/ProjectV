#version 460

// Fullscreen triangle vertex shader for the TAA resolve pass. We do not bind
// a vertex buffer; instead `gl_VertexIndex` (0..2) is consumed to emit a
// single oversized triangle that covers the entire swapchain. The fragment
// shader then derives UV from `gl_FragCoord` / swapchain extent.

void main()
{
    // -1..3 ranges: a single triangle covers [-1, 1] NDC plus one extra
    // pixel of overdraw on each side, which the rasterizer clips to the
    // exact framebuffer rect. The X coordinate is offset so the triangle
    // aligns to the left edge at index 0.
    const vec2 positionNdc = vec2(
        (gl_VertexIndex == 1u) ? 3.0 : -1.0,
        (gl_VertexIndex == 2u) ? 3.0 : -1.0);
    gl_Position = vec4(positionNdc, 0.0, 1.0);
}
