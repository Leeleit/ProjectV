// encoder_bc7.hpp — simplified BC7 mode 6 encoder/decoder.
// Reference: Microsoft Direct3D 11 BC7 + richgel999/bc7enc + Binomial basis_universal.
// MODE 6 only (simplified): single subset, 4-bit indices, 7.7.7.1 endpoints per channel.
//
// Block layout (128 bits):
//   endpoint0 R: 7 bits
//   endpoint0 G: 7 bits
//   endpoint0 B: 7 bits
//   endpoint1 R: 7 bits
//   endpoint1 G: 7 bits
//   endpoint1 B: 7 bits
//   P-bit: 1 bit (shared precision boost)
//   indices: 16 × 4 bits = 64 bits
// Total = 7*6 + 1 + 64 = 107 bits; padded to 128 bits (21 bits unused).
// 4x4 block, 128 bits = 8 bpp.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "scenes.hpp"

namespace texcomp {

struct BC7Block {
    std::uint8_t r0, g0, b0;
    std::uint8_t r1, g1, b1;
    std::uint8_t p;
    std::uint64_t indices;  // 16 × 4 bits = 64 bits
};

// Quantize 8-bit channel to 7-bit.
inline std::uint8_t Q7(std::uint8_t v) { return v >> 1; }
// Dequantize 7-bit to 8-bit.
inline std::uint8_t DQ7(std::uint8_t v) { return (v << 1) | (v >> 6); }
// Quantize 8-bit to 8-bit (no change).
inline std::uint8_t Q8(std::uint8_t v) { return v; }

inline std::uint8_t Interp4(std::uint8_t a, std::uint8_t b, int idx) {
    // Mode 6 interpolation weights for 4-bit index.
    // Per BC7 spec mode 6: weight = {0, 9, 18, 27, 37, 46, 55, 64, 64, 73, 82, 91, 100, 109, 118, 128} for indices 0..15.
    constexpr int w[16] = {0, 9, 18, 27, 37, 46, 55, 64, 64, 73, 82, 91, 100, 109, 118, 128};
    return (std::uint8_t)(((64 - w[idx]) * (int)a + w[idx] * (int)b) / 64);
}

inline BC7Block EncodeBc7Mode6Block(const RGBA8* pixels) {
    // Find min/max RGB per channel.
    std::uint8_t rmin = 255, gmin = 255, bmin = 255;
    std::uint8_t rmax = 0, gmax = 0, bmax = 0;
    for (int i = 0; i < 16; ++i) {
        rmin = std::min(rmin, pixels[i].r);
        gmin = std::min(gmin, pixels[i].g);
        bmin = std::min(bmin, pixels[i].b);
        rmax = std::max(rmax, pixels[i].r);
        gmax = std::max(gmax, pixels[i].g);
        bmax = std::max(bmax, pixels[i].b);
    }
    BC7Block blk{};
    // Try both p-bit values, pick best.
    std::uint16_t best_err = 255 * 16;
    for (int p_try = 0; p_try < 2; ++p_try) {
        // Scale endpoints by p-bit.
        std::uint8_t er0 = Q7(rmax);
        std::uint8_t eg0 = Q7(gmax);
        std::uint8_t eb0 = Q7(bmax);
        std::uint8_t er1 = Q7(rmin);
        std::uint8_t eg1 = Q7(gmin);
        std::uint8_t eb1 = Q7(bmin);
        // p-bit: scales endpoint range for higher precision when p=1.
        if (p_try == 1) {
            er0 = (er0 << 1) | 1;
            eg0 = (eg0 << 1) | 1;
            eb0 = (eb0 << 1) | 1;
            er1 = er1 << 1;
            eg1 = eg1 << 1;
            eb1 = eb1 << 1;
        }
        std::uint8_t ar0 = DQ7(er0);
        std::uint8_t ag0 = DQ7(eg0);
        std::uint8_t ab0 = DQ7(eb0);
        std::uint8_t ar1 = DQ7(er1);
        std::uint8_t ag1 = DQ7(eg1);
        std::uint8_t ab1 = DQ7(eb1);

        std::uint64_t indices = 0;
        std::uint16_t err = 0;
        for (int i = 0; i < 16; ++i) {
            int best_idx = 0;
            int best_d = 65536;
            for (int k = 0; k < 16; ++k) {
                std::uint8_t cr = Interp4(ar1, ar0, k);
                std::uint8_t cg = Interp4(ag1, ag0, k);
                std::uint8_t cb = Interp4(ab1, ab0, k);
                int dr = (int)pixels[i].r - (int)cr;
                int dg = (int)pixels[i].g - (int)cg;
                int db = (int)pixels[i].b - (int)cb;
                int d = dr * dr + dg * dg + db * db;
                if (d < best_d) { best_d = d; best_idx = k; }
            }
            err += (std::uint16_t)std::min(65535, best_d / 100);
            indices |= (std::uint64_t)best_idx << (i * 4);
        }
        if (err < best_err) {
            best_err = err;
            blk.r0 = er0;
            blk.g0 = eg0;
            blk.b0 = eb0;
            blk.r1 = er1;
            blk.g1 = eg1;
            blk.b1 = eb1;
            blk.p = (std::uint8_t)p_try;
            blk.indices = indices;
        }
    }
    return blk;
}

inline void DecodeBc7Mode6Block(const BC7Block& blk, RGBA8* pixels_out) {
    std::uint8_t er0 = blk.r0;
    std::uint8_t eg0 = blk.g0;
    std::uint8_t eb0 = blk.b0;
    std::uint8_t er1 = blk.r1;
    std::uint8_t eg1 = blk.g1;
    std::uint8_t eb1 = blk.b1;
    if (blk.p) {
        er0 = (er0 << 1) | 1;
        eg0 = (eg0 << 1) | 1;
        eb0 = (eb0 << 1) | 1;
        er1 = er1 << 1;
        eg1 = eg1 << 1;
        eb1 = eb1 << 1;
    }
    std::uint8_t ar0 = DQ7(er0);
    std::uint8_t ag0 = DQ7(eg0);
    std::uint8_t ab0 = DQ7(eb0);
    std::uint8_t ar1 = DQ7(er1);
    std::uint8_t ag1 = DQ7(eg1);
    std::uint8_t ab1 = DQ7(eb1);
    for (int i = 0; i < 16; ++i) {
        std::uint64_t idx = (blk.indices >> (i * 4)) & 0xF;
        pixels_out[i] = RGBA8{
            Interp4(ar1, ar0, (int)idx),
            Interp4(ag1, ag0, (int)idx),
            Interp4(ab1, ab0, (int)idx),
            255};
    }
}

inline std::vector<std::uint8_t> EncodeBc7(const AtlasImage& atlas,
                                            std::vector<RGBA8>& decoded_out) {
    int blocks_w = (atlas.width + 3) / 4;
    int blocks_h = (atlas.height + 3) / 4;
    std::vector<BC7Block> blocks;
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
            blocks[static_cast<std::size_t>(by) * blocks_w + bx] = EncodeBc7Mode6Block(block_pix);
        }
    }
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            RGBA8 block_pix[16];
            DecodeBc7Mode6Block(blocks[static_cast<std::size_t>(by) * blocks_w + bx], block_pix);
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
