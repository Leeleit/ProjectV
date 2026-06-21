// texture_formats.hpp — texture compression format specifications.
// Per Aras Pranckevičius "Texture Compression in 2020" + Wikipedia ASTC + Khronos spec.

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace texcomp {

enum class Format : int {
    Uncompressed = 0,  // RGBA8_UNORM (32 bpp)
    BC1 = 1,           // DXT1 (4 bpp, RGB+1bit alpha)
    BC3 = 2,           // DXT5 (8 bpp, RGBA)
    BC5 = 3,           // 2-channel (8 bpp, normal map)
    BC6H = 4,          // HDR RGB (8 bpp)
    BC7 = 5,           // LDR RGBA (8 bpp, best quality)
    ASTC_4x4 = 6,      // 8 bpp, mobile+desktop cross-vendor
    ASTC_6x6 = 7,      // 3.56 bpp, mobile+desktop
    ASTC_8x8 = 8,      // 2 bpp, mobile+desktop (lowest quality)
    ETC2_RGBA = 9,     // 8 bpp, mobile
};

struct FormatSpec {
    std::string name;
    int block_w;     // pixels per block, X
    int block_h;     // pixels per block, Y
    int bits_per_block;  // encoded block size in bits
    int channels;    // RGBA channels
    bool has_alpha;  // supports per-pixel alpha
    bool is_hdr;     // HDR format (BC6H)
    // Per Aras Pranckevičius "Texture Compression in 2020" benchmarks:
    double expected_psnr_db;  // average Luma PSNR vs uncompressed reference (smooth textures)
    double encode_speed_mpix_s;  // typical CPU encoder throughput
};

inline const std::array<FormatSpec, 10> kFormatSpecs = {{
    // Uncompressed reference.
    {"Uncompressed RGBA8", 1, 1, 32, 4, true, false,
     1000000000.0, 1000000000.0},  // PSNR = inf, throughput = passthrough (set high).

    // BC1 (DXT1) — 4 bpp RGB + 1bit alpha per 4x4 block.
    {"BC1 (DXT1)", 4, 4, 64, 3, false, false,
     38.5, 500.0},  // per Aras 2020: ~35-40 dB; ispc_texcomp ~500 Mpix/s

    // BC3 (DXT5) — 8 bpp RGBA.
    {"BC3 (DXT5)", 4, 4, 128, 4, true, false,
     40.0, 100.0},  // per Aras 2020: similar to BC1 RGB quality + alpha; ispc_texcomp ~100 Mpix/s

    // BC5 — 2-channel (normal map).
    {"BC5 (RG)", 4, 4, 128, 2, false, false,
     42.0, 200.0},  // canonical for normal maps per Narkowicz + ARAS

    // BC6H — HDR RGB.
    {"BC6H (HDR RGB)", 4, 4, 128, 3, false, true,
     40.0, 20.0},  // HDR; ~20 Mpix/s encoders; Binomial supports fast transcoding

    // BC7 — best LDR RGBA quality.
    {"BC7 (LDR RGBA)", 4, 4, 128, 4, true, false,
     48.0, 30.0},  // per Aras 2020: ~45-50 dB; bc7e encoder ~30 Mpix/s

    // ASTC 4x4 — 8 bpp.
    {"ASTC LDR 4x4", 4, 4, 128, 4, true, false,
     48.0, 8.0},  // per Aras 2020: similar quality to BC7 but slower encode

    // ASTC 6x6 — 3.56 bpp.
    {"ASTC LDR 6x6", 6, 6, 128, 4, true, false,
     38.0, 20.0},  // per Aras 2020: 35-40 dB

    // ASTC 8x8 — 2 bpp.
    {"ASTC LDR 8x8", 8, 8, 128, 4, true, false,
     32.0, 30.0},  // per Aras 2020: <35 dB, visible artifacts

    // ETC2 RGBA — 8 bpp.
    {"ETC2 RGBA", 4, 4, 128, 4, true, false,
     40.0, 4.0},  // per Aras 2020: ETC2 quality ~40 dB; ~4 Mpix/s typical encoder
}};

inline constexpr int kFormatCount = 10;

inline const FormatSpec& SpecOf(Format f) {
    return kFormatSpecs[static_cast<int>(f)];
}

inline double BitsPerPixel(Format f) {
    const auto& s = SpecOf(f);
    return static_cast<double>(s.bits_per_block) /
           static_cast<double>(s.block_w * s.block_h);
}

// Total atlas VRAM in MiB for uncompressed reference + mip chain.
// Mip chain: level 0 = N pixels, level k = N/8^k.
// VRAM(W,H) = (W*H*4) * (1 + 1/8 + 1/64 + ... + 1/8^N) where N ~ log2(min(W,H)).
inline double UncompressedAtlasMiB(int width, int height) {
    int mip_count = 0;
    int w = width, h = height;
    while (w > 1 || h > 1) {
        ++mip_count;
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }
    double total_pixels = 0;
    w = width; h = height;
    for (int k = 0; k <= mip_count; ++k) {
        total_pixels += static_cast<double>(w) * h;
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }
    return total_pixels * 4.0 / (1024.0 * 1024.0);
}

inline double CompressedAtlasMiB(Format f, int width, int height) {
    const auto& s = SpecOf(f);
    int blocks_w = (width + s.block_w - 1) / s.block_w;
    int blocks_h = (height + s.block_h - 1) / s.block_h;
    double total_bits = 0;
    int w = width, h = height;
    int bw = blocks_w, bh = blocks_h;
    while (w > 1 || h > 1) {
        total_bits += static_cast<double>(bw) * bh * s.bits_per_block;
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
        bw = (w + s.block_w - 1) / s.block_w;
        bh = (h + s.block_h - 1) / s.block_h;
        if (bw < 1) bw = 1;
        if (bh < 1) bh = 1;
    }
    total_bits += static_cast<double>(bw) * bh * s.bits_per_block;
    return total_bits / 8.0 / (1024.0 * 1024.0);
}

}  // namespace texcomp
