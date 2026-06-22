// 2026-06-21-save-game-persistence-architecture/prototype/compression.hpp
// LZ4-block compression with hash table (O(N) per chunk) + RLE for "zstd-like" Strategy C.
// Hash-table-based LZ4 is the standard LZ4 algorithm — single hash per 4-byte sequence →
// O(1) candidate lookup, then linear match extension. O(N) total per chunk instead of O(N²).

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace save_bench::compression {

inline constexpr std::size_t kLz4MinMatch = 4;
inline constexpr std::size_t kLz4MaxMatchLength = 15 + 15;  // 4 + 15 extra bytes = 19+4
inline constexpr std::uint32_t kHashSentinel = 0xFFFFFFFFu;

// FNV-1a 64-bit hash for content addressing.
inline auto fnv1a_64(std::span<const std::uint8_t> data) -> std::uint64_t {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (auto b : data) {
        h ^= b;
        h *= 0x100000001b3ULL;
    }
    return h;
}

// LZ4 block format: token (1 byte) + lit-len varint + literals + match-offset (LE16) + match-len varint.
// Reference: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
// Token: high 4 bits = literal length (0-15), low 4 bits = match length - MINMATCH (0-15).
// Varint: 255 bytes per extra length byte; final byte may be < 255.
// Last sequence: token with literal length 0 = end marker.

inline auto lz4_compress(std::span<const std::uint8_t> src) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> out;
    out.reserve(src.size() + src.size() / 255 + 16);

    if (src.size() < kLz4MinMatch + 1) {
        // Too small to match; emit as single literal sequence.
        const std::uint8_t token = static_cast<std::uint8_t>(src.size() << 4);
        out.push_back(token);
        for (auto b : src) out.push_back(b);
        out.push_back(0x00);  // end marker
        return out;
    }

    std::array<std::uint32_t, 1u << 14> hashTable{};
    hashTable.fill(kHashSentinel);

    auto read_u32 = [](const std::uint8_t* p) -> std::uint32_t {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    };

    std::size_t anchor = 0;
    std::size_t i = 0;

    while (i + kLz4MinMatch <= src.size()) {
        // Hash 4 bytes at i.
        const std::uint32_t hash = read_u32(src.data() + i);
        const std::uint32_t h = (hash * 2654435761u) >> 18;
        const std::uint32_t candidate = hashTable[h];
        hashTable[h] = static_cast<std::uint32_t>(i);

        std::size_t match_len = 0;
        std::size_t match_off = 0;

        if (candidate != kHashSentinel && i > candidate && i - candidate < 65535) {
            // Verify match (4-byte equality).
            if (src[candidate] == src[i] && src[candidate + 1] == src[i + 1] &&
                src[candidate + 2] == src[i + 2] && src[candidate + 3] == src[i + 3]) {
                match_off = i - candidate;
                match_len = kLz4MinMatch;
                while (match_len < kLz4MaxMatchLength && i + match_len < src.size() &&
                       src[candidate + match_len] == src[i + match_len]) {
                    ++match_len;
                }
            }
        }

        if (match_len >= kLz4MinMatch) {
            // Emit literal + match sequence.
            const std::size_t lit_len = i - anchor;
            const std::uint8_t lit_token = static_cast<std::uint8_t>(std::min<std::size_t>(lit_len, 15));
            const std::uint8_t match_token = static_cast<std::uint8_t>(std::min<std::size_t>(match_len - kLz4MinMatch, 15));
            out.push_back(static_cast<std::uint8_t>((lit_token << 4) | match_token));

            // Literal length varint (extra bytes if > 15).
            std::size_t lit_extra = lit_len - lit_token;
            while (lit_extra >= 255) {
                out.push_back(255);
                lit_extra -= 255;
            }
            if (lit_token == 15) {
                out.push_back(static_cast<std::uint8_t>(lit_extra));
            }

            // Literals.
            for (std::size_t k = 0; k < lit_len; ++k) {
                out.push_back(src[anchor + k]);
            }

            // Match offset (little-endian 16-bit).
            out.push_back(static_cast<std::uint8_t>(match_off & 0xFF));
            out.push_back(static_cast<std::uint8_t>((match_off >> 8) & 0xFF));

            // Match length varint.
            std::size_t match_extra = match_len - kLz4MinMatch - match_token;
            while (match_extra >= 255) {
                out.push_back(255);
                match_extra -= 255;
            }
            if (match_token == 15) {
                out.push_back(static_cast<std::uint8_t>(match_extra));
            }

            i += match_len;
            anchor = i;
        } else {
            ++i;
        }
    }

    // Final literal run + end marker.
    const std::size_t lit_len = src.size() - anchor;
    if (lit_len > 0) {
        const std::uint8_t lit_token = static_cast<std::uint8_t>(std::min<std::size_t>(lit_len, 15));
        out.push_back(static_cast<std::uint8_t>(lit_token << 4));
        std::size_t lit_extra = lit_len - lit_token;
        while (lit_extra >= 255) {
            out.push_back(255);
            lit_extra -= 255;
        }
        if (lit_token == 15) {
            out.push_back(static_cast<std::uint8_t>(lit_extra));
        }
        for (std::size_t k = 0; k < lit_len; ++k) {
            out.push_back(src[anchor + k]);
        }
    }
    out.push_back(0x00);  // end marker
    return out;
}

inline auto lz4_decompress(std::span<const std::uint8_t> src, std::size_t expected_size) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> out;
    out.reserve(expected_size);

    std::size_t i = 0;
    while (i < src.size()) {
        const std::uint8_t token = src[i++];
        std::size_t lit_len = (token >> 4) & 0xF;
        std::size_t match_len_base = token & 0xF;

        if (lit_len == 0 && i == src.size()) break;  // End marker.

        if (lit_len == 15) {
            while (i < src.size() && src[i] == 255) {
                lit_len += 255;
                ++i;
            }
            if (i < src.size()) {
                lit_len += src[i++];
            }
        }

        for (std::size_t k = 0; k < lit_len && out.size() < expected_size; ++k) {
            out.push_back(src[i++]);
        }

        if (i + 1 >= src.size()) break;  // No match offset = last sequence.

        const std::size_t match_off = static_cast<std::size_t>(src[i]) | (static_cast<std::size_t>(src[i + 1]) << 8);
        i += 2;
        std::size_t match_len = match_len_base + kLz4MinMatch;

        if (match_len_base == 15) {
            while (i < src.size() && src[i] == 255) {
                match_len += 255;
                ++i;
            }
            if (i < src.size()) {
                match_len += src[i++];
            }
        }

        for (std::size_t k = 0; k < match_len; ++k) {
            out.push_back(out[out.size() - match_off]);
        }
    }

    return out;
}

// RLE compressor (run-length encoding) — O(N) per chunk. Trivial but effective on uniform data.
// Used as fallback for Strategy C (zstd-like) on data that doesn't benefit from LZ4.
// Format: [count: u16, value: u8] repeated. count 0 = end-of-data. count 65535 = 65535 copies.
// Use only for runs of identical bytes; for mixed data the overhead exceeds savings.
inline auto rle_compress(std::span<const std::uint8_t> src) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> out;
    out.reserve(src.size() / 2 + 16);

    std::size_t i = 0;
    while (i < src.size()) {
        std::uint8_t value = src[i];
        std::size_t run = 1;
        while (i + run < src.size() && src[i + run] == value && run < 65535) ++run;
        out.push_back(static_cast<std::uint8_t>(run & 0xFF));
        out.push_back(static_cast<std::uint8_t>((run >> 8) & 0xFF));
        out.push_back(value);
        i += run;
    }
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x00);  // end marker
    return out;
}

inline auto rle_decompress(std::span<const std::uint8_t> src) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> out;
    std::size_t i = 0;
    while (i + 2 < src.size()) {
        const std::size_t count = static_cast<std::size_t>(src[i]) | (static_cast<std::size_t>(src[i + 1]) << 8);
        if (count == 0) break;
        const std::uint8_t value = src[i + 2];
        for (std::size_t k = 0; k < count; ++k) out.push_back(value);
        i += 3;
    }
    return out;
}

// Adaptive compressor: choose LZ4 (better for mixed data) or RLE (better for uniform).
// For voxel data with sparse fill, LZ4 wins on mixed regions. RLE wins on uniform_air.
inline auto adaptive_compress(std::span<const std::uint8_t> src) -> std::vector<std::uint8_t> {
    // Heuristic: count runs of identical bytes; if >50% uniform, RLE; else LZ4.
    std::size_t uniform_bytes = 0;
    if (src.size() < 2) return lz4_compress(src);
    std::size_t run = 1;
    for (std::size_t i = 1; i < src.size(); ++i) {
        if (src[i] == src[i - 1]) {
            ++run;
        } else {
            if (run >= 16) uniform_bytes += run;
            run = 1;
        }
    }
    if (run >= 16) uniform_bytes += run;
    if (uniform_bytes * 2 > src.size()) {
        return rle_compress(src);
    }
    return lz4_compress(src);
}

inline auto adaptive_decompress(std::span<const std::uint8_t> src, std::size_t expected_size) -> std::vector<std::uint8_t> {
    // Heuristic: if compressed is much smaller than expected * 0.4, it's RLE.
    if (src.size() * 10 < expected_size * 4) {
        auto rle_result = rle_decompress(src);
        if (rle_result.size() == expected_size) return rle_result;
    }
    return lz4_decompress(src, expected_size);
}

}  // namespace save_bench::compression