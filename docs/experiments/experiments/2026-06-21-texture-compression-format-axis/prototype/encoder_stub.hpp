// encoder_stub.hpp — analytical VRAM cost + Aras benchmark projection for formats
// not implemented as real encoder in this prototype.
// Covers: BC6H (HDR), ASTC 4x4/6x6/8x8, ETC2 RGBA.
//
// Source: Aras Pranckevičius "Texture Compression in 2020"
// (aras-p.info/blog/2020/12/08/Texture-Compression-in-2020/) — measured Luma PSNR
// for representative texture corpora; Binomial basis_universal README
// (github.com/BinomialLLC/basis_universal) — encoder quality data.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "scenes.hpp"
#include "texture_formats.hpp"

namespace texcomp {

// Stub encoder: returns VRAM cost bytes (no real encoding), copies atlas as decoded.
// For formats not directly implemented, we apply simulated quantization to estimate
// quality loss using Aras' benchmark numbers + per-format quantization error model.
inline std::vector<std::uint8_t> EncodeStub(const AtlasImage& atlas, Format fmt,
                                             std::vector<RGBA8>& decoded_out,
                                             double projected_psnr_db) {
    const auto& s = SpecOf(fmt);
    int blocks_w = (atlas.width + s.block_w - 1) / s.block_w;
    int blocks_h = (atlas.height + s.block_h - 1) / s.block_h;
    int total_blocks = blocks_w * blocks_h;
    int bytes_per_block = s.bits_per_block / 8;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(total_blocks) * bytes_per_block, 0);

    // For stub decoders, simulate quality loss by adding AWGN per pixel.
    // Standard deviation derived from PSNR: PSNR = 10*log10(255^2/MSE) => MSE = 255^2/10^(PSNR/10).
    // For per-pixel independent Gaussian noise with variance sigma^2 = MSE/3 (RGB).
    if (projected_psnr_db <= 0 || std::isinf(projected_psnr_db)) {
        // Perfect passthrough.
        decoded_out = atlas.pixels;
        return bytes;
    }
    double mse = (255.0 * 255.0) / std::pow(10.0, projected_psnr_db / 10.0);
    double sigma = std::sqrt(mse / 3.0);
    decoded_out.resize(atlas.pixels.size());
    std::mt19937 rng(static_cast<std::uint32_t>(atlas.width * 65537 + atlas.height));
    std::normal_distribution<double> dist(0.0, sigma);
    for (std::size_t i = 0; i < atlas.pixels.size(); ++i) {
        const auto& p = atlas.pixels[i];
        decoded_out[i].r = (std::uint8_t)std::clamp((int)std::round(p.r + dist(rng)), 0, 255);
        decoded_out[i].g = (std::uint8_t)std::clamp((int)std::round(p.g + dist(rng)), 0, 255);
        decoded_out[i].b = (std::uint8_t)std::clamp((int)std::round(p.b + dist(rng)), 0, 255);
        decoded_out[i].a = p.a;
    }
    return bytes;
}

}  // namespace texcomp
