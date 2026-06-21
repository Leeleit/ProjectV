// encoder_uncompressed.hpp — pass-through (no compression).
// Used as PSNR reference baseline.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "scenes.hpp"

namespace texcomp {

inline std::vector<std::uint8_t> EncodeUncompressed(const AtlasImage& atlas,
                                                     std::vector<RGBA8>& decoded_out) {
    std::vector<std::uint8_t> out;
    out.resize(atlas.size_bytes());
    std::memcpy(out.data(), atlas.pixels.data(), atlas.size_bytes());
    decoded_out = atlas.pixels;
    return out;
}

}  // namespace texcomp
