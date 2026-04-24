#version 460

layout(location = 0) flat in uint inMaterialIndex;

const uint kGlassMaterial = 1u;

void main() {
    if (inMaterialIndex == kGlassMaterial) {
        discard;
    }
}
