// encoder_bc5.hpp — simplified BC5 (2-channel) encoder/decoder.
// = 2x BC1 (R = X, G = Y) for normal maps. Z reconstructed in shader: Z = sqrt(1 - X² - Y²).
// Block = 128 bits per 4x4 = 8 bpp. Normal map canonical per Narkowicz + ARAS Pranckevičius.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "encoder_bc1.hpp"
#include "scenes.hpp"

namespace texcomp {

struct BC5Block {
    BC1Block ch_r;
    BC1Block ch_g;
};

inline BC5Block EncodeBc5Block(const RGBA8* pixels) {
    BC5Block blk{};
    // Pack R/G channels into BC1 RGB-only streams.
    RGBA8 rg[16];
    RGBA8 gg[16];
    for (int i = 0; i < 16; ++i) {
        rg[i] = RGBA8{pixels[i].r, 0, 0, 255};
        gg[i] = RGBA8{pixels[i].g, 0, 0, 255};
    }
    blk.ch_r = EncodeBc1Block(rg);
    blk.ch_g = EncodeBc1Block(gg);
    return blk;
}

inline void DecodeBc5Block(const BC5Block& blk, RGBA8* pixels_out) {
    RGBA8 tmp[16];
    DecodeBc1Block(blk.ch_r, tmp);
    for (int i = 0; i < 16; ++i) pixels_out[i].r = tmp[i].r;
    DecodeBc1Block(blk.ch_g, tmp);
    for (int i = 0; i < 16; ++i) pixels_out[i].g = tmp[i].g;
    // B channel reconstructed (Z) in shader per canonical pattern.
    for (int i = 0; i < 16; ++i) {
        pixels_out[i].b = 255;  // placeholder; shader reconstructs from r/g
        pixels_out[i].a = 255;
    }
}

inline std::vector<std::uint8_t> EncodeBc5(const AtlasImage& atlas,
                                            std::vector<RGBA8>& decoded_out) {
    int blocks_w = (atlas.width + 3) / 4;
    int blocks_h = (atlas.height + 3) / 4;
    std::vector<BC5Block> blocks;
    blocks.resize(static_cast<std::size_t>(blocks_w) * blocks_h);
    decoded_out.assign(static_cast<std::size_t>(atlas.width) * atlas.height, RGBA8{0, 0, 0, 255});
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            RGBA8 block_pix[16];
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    if (x >= atlas.width || y >= atlas.height) {
                        block_pix[py * 4 + px] = RGBA8{0, 0, 0, 255};
                    } else {
                        block_pix[py * 4 + px] = atlas.pixels[static_cast<std::size_t>(y) * atlas.width + x];
                    }
                }
            }
            blocks[static_cast<std::size_t>(by) * blocks_w + bx] = EncodeBc5Block(block_pix);
        }
    }
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            RGBA8 block_pix[16];
            DecodeBc5Block(blocks[static_cast<std::size_t>(by) * blocks_w + bx], block_pix);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    if (x >= atlas.width || y >= atlas.height) continue;
                    decoded_out[static_cast<std::size_t>(y) * atlas.width + x] = block_pix[py * 4 + px];
                }
            }
        }
    }
    std::vector<std::uint8_t> bytes(blocks.size() * 16);
    std::memcpy(bytes.data(), blocks.data(), bytes.size());
    return bytes;
}

}  // namespace texcomp
