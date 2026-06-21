#version 460
// SPDX-License-Identifier: MIT
// ProjectV — sky atmosphere vertex shader (full-screen tri)
//
// Per Hillaire 2020 EGSR + sebh/UnrealEngineSkyAtmosphere reference. MVP: no
// LUT precomputation, no Rayleigh/Mie analytical formula — Phase 2 emits a
// full-screen tri that the fragment shader turns into the sky background.
// Phase 3 will replace the analytical in-shader color with a Sky-View LUT
// lookup + multi-scattering approximation.

layout(location = 0) out vec2 outNdcXY;

void main() {
    const float x = float((gl_VertexIndex & 1) << 2);
    const float y = float((gl_VertexIndex & 2) << 1);
    outNdcXY = vec2(x, y);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.9999, 1.0);
}
