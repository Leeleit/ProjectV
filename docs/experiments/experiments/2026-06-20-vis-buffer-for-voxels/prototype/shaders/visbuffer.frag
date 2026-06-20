#version 460
// Fragment shader (VIS-BUFFER geometry pass).
// Just writes quadId to the vis-buffer RT. That's all the per-pixel work.
// Total per-pixel cost: 1 uint store. No ALU beyond that.

layout(location = 0) flat in uint inQuadId;
layout(location = 0, index = 0) out uint outVisBuffer;

void main() {
    outVisBuffer = inQuadId;
}
