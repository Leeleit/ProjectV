#version 460
// SPDX-License-Identifier: MIT
// ProjectV — cloudscape vertex shader (full-screen tri).
//
// Per Schneider "Nubis" 2017 + Wronski 2014 + closed
// `2026-06-21-cloudscape-rendering` experiment (B_SingleLayerRayMarch
// universal default). MVP: no multi-layer, no shadow self-cast, no FBM
// detail noise — single ray-march through a flat horizontal slab.

layout(location = 0) out vec2 outNdcXY;

void main() {
    const float x = float((gl_VertexIndex & 1) << 2);
    const float y = float((gl_VertexIndex & 2) << 1);
    outNdcXY = vec2(x, y);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.0, 1.0);
}
