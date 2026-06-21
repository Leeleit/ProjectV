// encoder_bc3.hpp — simplified BC3 (DXT5) encoder/decoder.
// = BC1 RGB (64 bits) + alpha block (64 bits) = 128 bits per 4x4 block = 8 bpp.
// Alpha block: alpha0 (8 bits) + alpha1 (8 bits) + 16 indices (3 bits each, 48 bits).
// 8 alpha levels per Microsoft BC3 spec.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "encoder_bc1.hpp"
#include "scenes.hpp"

namespace texcomp {

struct BC3Block {
    BC1Block rgb;
    std::uint8_t alpha0;
    std::uint8_t alpha1;
    std::uint64_t alpha_indices;  // 16 × 3 bits = 48 bits
};

inline std::uint8_t Bc3AlphaLevel(int i, std::uint8_t a0, std::uint8_t a1) {
    if (a0 > a1) {
        switch (i) {
            case 0: return a0;
            case 1: return a1;
            case 2: return (std::uint8_t)((6 * a0 + 1 * a1) / 7);
            case 3: return (std::uint8_t)((5 * a0 + 2 * a1) / 7);
            case 4: return (std::uint8_t)((4 * a0 + 3 * a1) / 7);
            case 5: return (std::uint8_t)((3 * a0 + 4 * a1) / 7);
            case 6: return (std::uint8_t)((2 * a0 + 5 * a1) / 7);
            case 7: return (std::uint8_t)((1 * a0 + 6 * a1) / 7);
        }
    } else {
        switch (i) {
            case 0: return a0;
            case 1: return a1;
            case 2: return (std::uint8_t)((4 * a0 + 1 * a1) / 5);
            case 3: return (std::uint8_t)((3 * a0 + 2 * a1) / 5);
            case 4: return (std::uint8_t)((2 * a0 + 3 * a1) / 5);
            case 5: return (std::uint8_t)((1 * a0 + 4 * a1) / 5);
            case 6: return 0;
            case 7: return 255;
        }
    }
    return 0;
}

inline BC3Block EncodeBc3Block(const RGBA8* pixels) {
    BC3Block blk{};
    blk.rgb = EncodeBc1Block(pixels);
    std::uint8_t amin = 255, amax = 0;
    for (int i = 0; i < 16; ++i) {
        amin = std::min(amin, pixels[i].a);
        amax = std::max(amax, pixels[i].a);
    }
    blk.alpha0 = amax;
    blk.alpha1 = amin;
    std::uint64_t indices = 0;
    for (int i = 0; i < 16; ++i) {
        int best = 0;
        int best_d = std::abs((int)pixels[i].a - (int)Bc3AlphaLevel(0, blk.alpha0, blk.alpha1));
        for (int k = 1; k < 8; ++k) {
            int d = std::abs((int)pixels[i].a - (int)Bc3AlphaLevel(k, blk.alpha0, blk.alpha1));
            if (d < best_d) { best_d = d; best = k; }
        }
        indices |= (std::uint64_t)best << (i * 3);
    }
    blk.alpha_indices = indices;
    return blk;
}

inline void DecodeBc3Block(const BC3Block& blk, RGBA8* pixels_out) {
    DecodeBc1Block(blk.rgb, pixels_out);
    for (int i = 0; i < 16; ++i) {
        std::uint64_t idx = (blk.alpha_indices >> (i * 3)) & 0x7;
        pixels_out[i].a = Bc3AlphaLevel((int)idx, blk.alpha0, blk.alpha1);
    }
}

inline std::vector<std::uint8_t> EncodeBc3(const AtlasImage& atlas,
                                            std::vector<RGBA8>& decoded_out) {
    int blocks_w = (atlas.width + 3) / 4;
    int blocks_h = (atlas.height + 3) / 4;
    std::vector<BC3Block> blocks;
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
            blocks[static_cast<std::size_t>(by) * blocks_w + bx] = EncodeBc3Block(block_pix);
        }
    }
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            RGBA8 block_pix[16];
            DecodeBc3Block(blocks[static_cast<std::size_t>(by) * blocks_w + bx], block_pix);
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
