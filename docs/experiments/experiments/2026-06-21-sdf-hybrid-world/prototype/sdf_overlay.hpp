#pragma once

#include "scenes.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace sdf_hybrid::sdf {

using scenes::CHUNK_VOLUME;
using scenes::CHUNK_SIZE;

// SDF encoding schemes.
//
// All schemes produce an 8-bit per-voxel storage:
//   - bit 7: sign (0 = positive = outside, 1 = negative = inside)
//   - bits 0-6: distance magnitude (0-127, but capped to chunkSize-1 = 7 for our case)
//
// A_None: no SDF (baseline)
// B_R8_1byte: standard 8-bit SDF
// C_R8_4quant: 4 quantized distance values (saves 2 bits, uses 2-bit enum + 6-bit sign+mag)
// D_RLE_NoneSparse: run-length encoded for empty regions
enum class SdfEncoding : std::uint8_t {
    A_None = 0,         // baseline: no SDF
    B_R8_1byte,         // 1 byte/voxel: 7-bit distance + 1-bit sign
    C_R8_4quant,         // 1 byte/voxel: 6-bit distance + 2-bit quantization index
    D_RLE_NoneSparse,    // 1 byte/voxel: RLE for "far from surface" cells (RLE marker)
};

constexpr const char* encoding_name(SdfEncoding e) noexcept {
    switch (e) {
        case SdfEncoding::A_None:         return "A_None";
        case SdfEncoding::B_R8_1byte:     return "B_R8_1byte";
        case SdfEncoding::C_R8_4quant:    return "C_R8_4quant";
        case SdfEncoding::D_RLE_NoneSparse: return "D_RLE_NoneSparse";
    }
    return "unknown";
}

// SDF storage: 8-bit per voxel (in mainline format).
using SdfR8 = std::array<std::uint8_t, CHUNK_VOLUME>;

// Maximum distance stored in 7-bit = 127, but practically we use 0..7 (chunkSize-1).
inline constexpr std::uint8_t SDF_MAX_DIST = 7;  // chunkSize - 1
inline constexpr std::uint8_t SDF_SIGN_BIT = 0x80;
inline constexpr std::uint8_t SDF_DIST_MASK = 0x7F;

// Pack distance + sign into a single byte (B_R8_1byte).
inline constexpr std::uint8_t pack_sdf(bool inside, std::uint8_t dist) noexcept {
    return (inside ? SDF_SIGN_BIT : 0) | (dist & SDF_DIST_MASK);
}

inline constexpr bool sdf_inside(std::uint8_t sdf) noexcept {
    return (sdf & SDF_SIGN_BIT) != 0;
}

inline constexpr std::uint8_t sdf_dist(std::uint8_t sdf) noexcept {
    return sdf & SDF_DIST_MASK;
}

// Decode signed distance: positive = outside, negative = inside.
inline constexpr int sdf_value(std::uint8_t sdf) noexcept {
    std::uint8_t d = sdf_dist(sdf);
    return sdf_inside(sdf) ? -static_cast<int>(d) : static_cast<int>(d);
}

// Build algorithms for SDF generation from binary voxel grid.
enum class SdfBuild : std::uint8_t {
    J_JFA_GPU = 0,            // Jump Flooding Algorithm (Rong 2006) — fast, GPU-friendly
    K_BruteForce_BFS,         // Naive BFS — baseline for comparison
    L_AdaptiveMultiRes,        // Lower resolution for far-from-surface cells (placeholder, similar to JFA)
};

constexpr const char* build_name(SdfBuild b) noexcept {
    switch (b) {
        case SdfBuild::J_JFA_GPU:           return "J_JFA_GPU";
        case SdfBuild::K_BruteForce_BFS:    return "K_BruteForce_BFS";
        case SdfBuild::L_AdaptiveMultiRes:   return "L_AdaptiveMultiRes";
    }
    return "unknown";
}

// Generate SDF for the given voxel chunk.
// Output: sdf[i] = packed(inside, distance_to_surface).
// For A_None encoding: sdf is not populated; caller can use A_None baseline (skip computation).
void generate_jfa(const scenes::Chunk& voxels, SdfR8& sdf) noexcept;

void generate_brute_force_bfs(const scenes::Chunk& voxels, SdfR8& sdf) noexcept;

void generate_adaptive_multires(const scenes::Chunk& voxels, SdfR8& sdf) noexcept;

// Dispatcher.
void generate(SdfBuild build, const scenes::Chunk& voxels, SdfR8& sdf) noexcept;

// Compute compressed VRAM size for the encoding.
std::size_t vram_bytes(SdfEncoding enc, const scenes::Chunk& voxels) noexcept;

// Compute the "is this voxel on the surface?" check (for VCT termination reference).
// Surface = at least one 6-neighbor is different material (including air).
bool is_surface(const scenes::Chunk& voxels, std::size_t i) noexcept;

}  // namespace sdf_hybrid::sdf
