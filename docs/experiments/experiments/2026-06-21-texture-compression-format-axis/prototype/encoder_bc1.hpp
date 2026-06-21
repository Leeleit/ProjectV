// encoder_bc1.hpp — simplified BC1 (DXT1) encoder/decoder.
// Reference: Microsoft Direct3D 11 BC1 + richgel999/bc7enc rgbcx.h
// (prioritized cluster fit, simplified version without SSE/AVX optimization).
//
// BC1 block = 4x4 pixels, 64 bits = 8 bytes per block = 4 bpp.
// color0 (RGB565, 16 bits) + color1 (RGB565, 16 bits) + 16 indices (2 bits each, 32 bits).
// 4-color mode (color0 > color1 in RGB565 numeric): color0, color1, 2/3*color0 + 1/3*color1, 1/3*color0 + 2/3*color1.
// 3-color + transparent mode (color0 ≤ color1): color0, color1, 1/2*color0 + 1/2*color1, transparent.

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "scenes.hpp"

namespace texcomp {

// ---- RGB565 helpers ----
inline std::uint16_t Rgb565(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return (std::uint16_t)((r >> 3) << 11) | (std::uint16_t)((g >> 2) << 5) | (std::uint16_t)(b >> 3);
}

inline void UnpackRgb565(std::uint16_t v, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    // Expand 5/6/5-bit channels back to 8-bit (top bits replicated).
    std::uint8_t r5 = (v >> 11) & 0x1F;
    std::uint8_t g6 = (v >> 5) & 0x3F;
    std::uint8_t b5 = v & 0x1F;
    r = (r5 << 3) | (r5 >> 2);
    g = (g6 << 2) | (g6 >> 4);
    b = (b5 << 3) | (b5 >> 2);
}

// ---- 4-color and 3-color interpolation ----
inline RGBA8 Lerp3(const RGBA8& a, const RGBA8& b) {
    return RGBA8{(std::uint8_t)((a.r + b.r) / 2), (std::uint8_t)((a.g + b.g) / 2),
                 (std::uint8_t)((a.b + b.b) / 2), 255};
}
inline RGBA8 Lerp13(const RGBA8& a, const RGBA8& b) {
    // 1/3 A + 2/3 B
    return RGBA8{(std::uint8_t)((a.r + 2 * b.r) / 3), (std::uint8_t)((a.g + 2 * b.g) / 3),
                 (std::uint8_t)((a.b + 2 * b.b) / 3), 255};
}
inline RGBA8 Lerp23(const RGBA8& a, const RGBA8& b) {
    return RGBA8{(std::uint8_t)((2 * a.r + b.r) / 3), (std::uint8_t)((2 * a.g + b.g) / 3),
                 (std::uint8_t)((2 * a.b + b.b) / 3), 255};
}

// ---- BC1 encoder (simplified): find dominant endpoints, assign indices ----
struct BC1Block {
    std::uint16_t c0;
    std::uint16_t c1;
    std::uint32_t indices;  // 16 × 2 bits packed
};

inline BC1Block EncodeBc1Block(const RGBA8* pixels) {
    // Find min/max RGB across 16 pixels (ignore alpha for opaque mode).
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
    BC1Block blk{};
    blk.c0 = Rgb565(rmax, gmax, bmax);
    blk.c1 = Rgb565(rmin, gmin, bmin);

    RGBA8 ca{}, cb{};
    UnpackRgb565(blk.c0, ca.r, ca.g, ca.b);
    UnpackRgb565(blk.c1, cb.r, cb.g, cb.b);
    ca.a = 255;
    cb.a = 255;

    RGBA8 c2 = Lerp13(ca, cb);  // 1/3 A + 2/3 B
    RGBA8 c3 = Lerp23(ca, cb);  // 2/3 A + 1/3 B

    std::uint32_t indices = 0;
    for (int i = 0; i < 16; ++i) {
        // Pick closest color in {c0, c1, c2, c3}.
        auto dist2 = [](const RGBA8& a, const RGBA8& b) {
            int dr = (int)a.r - (int)b.r;
            int dg = (int)a.g - (int)b.g;
            int db = (int)a.b - (int)b.b;
            return dr * dr + dg * dg + db * db;
        };
        std::uint32_t best = 0;
        int best_d = dist2(pixels[i], ca);
        int d = dist2(pixels[i], cb);
        if (d < best_d) { best_d = d; best = 1; }
        d = dist2(pixels[i], c2);
        if (d < best_d) { best_d = d; best = 2; }
        d = dist2(pixels[i], c3);
        if (d < best_d) { best_d = d; best = 3; }
        indices |= best << (i * 2);
    }
    blk.indices = indices;
    return blk;
}

inline void DecodeBc1Block(const BC1Block& blk, RGBA8* pixels_out) {
    RGBA8 ca{}, cb{};
    UnpackRgb565(blk.c0, ca.r, ca.g, ca.b);
    UnpackRgb565(blk.c1, cb.r, cb.g, cb.b);
    ca.a = 255;
    cb.a = 255;
    RGBA8 c2 = Lerp13(ca, cb);
    RGBA8 c3 = Lerp23(ca, cb);
    for (int i = 0; i < 16; ++i) {
        std::uint32_t idx = (blk.indices >> (i * 2)) & 0x3;
        switch (idx) {
            case 0: pixels_out[i] = ca; break;
            case 1: pixels_out[i] = cb; break;
            case 2: pixels_out[i] = c2; break;
            case 3: pixels_out[i] = c3; break;
        }
    }
}

// ---- Atlas encode/decode ----
inline std::vector<std::uint8_t> EncodeBc1(const AtlasImage& atlas,
                                            std::vector<RGBA8>& decoded_out) {
    int blocks_w = (atlas.width + 3) / 4;
    int blocks_h = (atlas.height + 3) / 4;
    std::vector<BC1Block> blocks;
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
            blocks[static_cast<std::size_t>(by) * blocks_w + bx] = EncodeBc1Block(block_pix);
        }
    }
    // Decode for PSNR measurement.
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            RGBA8 block_pix[16];
            DecodeBc1Block(blocks[static_cast<std::size_t>(by) * blocks_w + bx], block_pix);
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
    std::vector<std::uint8_t> bytes(blocks.size() * 8);
    std::memcpy(bytes.data(), blocks.data(), bytes.size());
    return bytes;
}

}  // namespace texcomp
