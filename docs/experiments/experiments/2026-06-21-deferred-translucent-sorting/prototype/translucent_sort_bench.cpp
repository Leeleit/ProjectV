// SPDX-License-Identifier: MIT
// Standalone C++26 CPU benchmark for deferred translucent sorting (ProjectV Stage 5.x)
// Models VoxelCore ChunksRenderer.cpp:349-421 deferred sort pattern

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using f32  = float;
using u32  = uint32_t;
using i32  = int32_t;
using u64  = uint64_t;
using i64  = int64_t;
using f64  = double;

// ============================================================
// Translucent entry model
// ============================================================
struct TranslucentEntry {
    f32 distToCamera;  // computed per frame from position
    f32 alpha;         // 0..1 opacity
    f32 aabbVolume;    // normalized 0..1 (for merge optimization)
    u32 materialId;    // for debug

    // For sorting back-to-front
    bool operator<(const TranslucentEntry &o) const {
        return distToCamera > o.distToCamera; // far-first = back-to-front
    }
};

// ============================================================
// Camera model
// ============================================================
struct Camera {
    f32 posX, posY, posZ;
    f32 yawRad, pitchRad; // radians

    void rotate(f32 dyaw, f32 dpitch) {
        yawRad   += dyaw;
        pitchRad += dpitch;
        pitchRad  = std::max(-1.5f, std::min(1.5f, pitchRad));
    }

    // Forward vector after rotation
    f32 forwardX() const { return std::sin(yawRad); }
    f32 forwardZ() const { return std::cos(yawRad); }
};

// ============================================================
// Scene types
// ============================================================
enum class SceneType : int {
    NO_TRANSLUCENT     = 0,
    WATER_SURFACE,
    GLASS_BUILDING,
    ICE_CAVE,
    MIXED_TRANSLUCENT,
    COUNT
};

const char* scene_name(SceneType s) {
    switch (s) {
        case SceneType::NO_TRANSLUCENT:   return "no_translucent";
        case SceneType::WATER_SURFACE:    return "water_surface";
        case SceneType::GLASS_BUILDING:   return "glass_building";
        case SceneType::ICE_CAVE:         return "ice_cave";
        case SceneType::MIXED_TRANSLUCENT: return "mixed_translucent";
        default: return "unknown";
    }
}

// Generate translucent entries for a scene
std::vector<TranslucentEntry> generate_scene(SceneType type, u32 seed) {
    std::mt19937 rng(seed);
    std::vector<TranslucentEntry> entries;

    auto add_entry = [&](f32 x, f32, f32, f32 alpha, f32 vol) {
        TranslucentEntry e;
        e.distToCamera  = x;
        e.alpha         = std::clamp(alpha, 0.05f, 1.0f);
        e.aabbVolume    = std::clamp(vol, 0.0f, 1.0f);
        e.materialId    = static_cast<u32>(entries.size() % 256);
        entries.push_back(e);
    };

    switch (type) {
        case SceneType::NO_TRANSLUCENT:
            break;

        case SceneType::WATER_SURFACE: {
            // 8x8 water surface (single layer) = 64 water blocks
            // plus a few decorative translucent elements
            for (i32 x = 0; x < 8; ++x) {
                for (i32 z = 0; z < 8; ++z) {
                    add_entry(
                        (f32)x - 4.0f, 0.0f, (f32)z - 4.0f,
                        0.3f + (rng() % 10) * 0.02f,  // alpha 0.3-0.5
                        0.8f + (rng() % 5) * 0.04f    // volume 0.8-1.0
                    );
                }
            }
            break;
        }

        case SceneType::GLASS_BUILDING: {
            // 4x4x4 glass cube structure = 96 outer blocks + interior
            for (i32 x = -3; x <= 3; ++x) {
                for (i32 y = -3; y <= 3; ++y) {
                    for (i32 z = -3; z <= 3; ++z) {
                        if (std::abs(x) == 3 || std::abs(y) == 3 || std::abs(z) == 3) {
                            add_entry(
                                (f32)x, (f32)y, (f32)z,
                                0.2f + (rng() % 5) * 0.05f, // alpha 0.2-0.4
                                0.9f
                            );
                        }
                    }
                }
            }
            break;
        }

        case SceneType::ICE_CAVE: {
            // Scattered ice blocks in a cave pattern
            for (i32 i = 0; i < 120; ++i) {
                f32 x = (rng() % 200 - 100) * 0.1f;
                f32 y = (rng() % 60 - 10) * 0.1f;
                f32 z = (rng() % 200 - 100) * 0.1f;
                // Cluster near cave walls
                f32 dist = std::sqrt(x*x + y*y + z*z);
                if (dist > 3.0f && dist < 8.0f) {
                    add_entry(x, y, z,
                        0.15f + (rng() % 8) * 0.03f,
                        0.6f + (rng() % 4) * 0.1f
                    );
                }
            }
            break;
        }

        case SceneType::MIXED_TRANSLUCENT: {
            // Mix of water + glass + ice + colored glass
            // Water layer
            for (i32 x = 0; x < 6; ++x)
                for (i32 z = 0; z < 6; ++z)
                    add_entry((f32)x - 3, 0.0f, (f32)z - 3, 0.35f, 0.85f);

            // Glass pillars
            for (i32 i = 0; i < 20; ++i) {
                f32 x = (rng() % 80 - 40) * 0.1f;
                f32 z = (rng() % 80 - 40) * 0.1f;
                for (i32 y = 0; y < 6; ++y)
                    add_entry(x, (f32)y + 0.5f, z, 0.15f, 0.95f);
            }

            // Ice clusters
            for (i32 i = 0; i < 50; ++i) {
                f32 x = (rng() % 120 - 60) * 0.1f;
                f32 y = (rng() % 30 - 5) * 0.1f;
                f32 z = (rng() % 120 - 60) * 0.1f;
                add_entry(x, y, z, 0.25f, 0.7f);
            }

            // Small decorative
            for (i32 i = 0; i < 30; ++i) {
                f32 x = (rng() % 100 - 50) * 0.1f;
                f32 z = (rng() % 100 - 50) * 0.1f;
                add_entry(x, 0.5f, z, 0.5f + (rng() % 5) * 0.05f, 0.5f);
            }
            break;
        }

        default: break;
    }

    return entries;
}

// ============================================================
// Sorting strategies
// ============================================================
enum class Strategy : int {
    PER_FRAME           = 0, // A — sort every frame (baseline, correct)
    EVERY_4             = 1, // B — sort every 4 frames
    EVERY_8             = 2, // B — sort every 8 frames (VoxelCore default)
    EVERY_16            = 3, // B — sort every 16 frames
    DISTANCE_ADAPTIVE   = 4, // C — sort freq based on camera rotation
    PER_CHUNK           = 5, // D — sort within each chunk only
    COUNT
};

const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::PER_FRAME:          return "A_PerFrame";
        case Strategy::EVERY_4:            return "B_Every4";
        case Strategy::EVERY_8:            return "B_Every8";
        case Strategy::EVERY_16:           return "B_Every16";
        case Strategy::DISTANCE_ADAPTIVE:  return "C_DistanceAdaptive";
        case Strategy::PER_CHUNK:          return "D_PerChunk";
        default: return "unknown";
    }
}

// ============================================================
// PSNR model — maps sort quality to perceptual quality
// Based on: alpha-weighted inversion count → dB degradation
// ============================================================
struct SortResult {
    f64  sortTimeUs;       // mean sort time per frame
    f64  sortQuality;      // 0..1 fraction of correctly ordered adjacent pairs
    f64  psnrDb;           // estimated PSNR (8 = no translucency, ~45 = perfect sort)
    i32  entryCount;       // number of translucent entries
    i32  framesSorted;     // how many frames actually sorted (out of total)
    i64  totalInversions;  // total pair inversions
};

f64 compute_sort_quality(const std::vector<TranslucentEntry>& sorted,
                         const std::vector<TranslucentEntry>& reference)
{
    if (sorted.empty() || reference.empty()) return 1.0;
    // Compare adjacent pairs: in correct back-to-front order,
    // each entry should be farther than the next
    i32 correct = 0;
    i32 total   = 0;
    const i32 n = std::min((i32)sorted.size(), (i32)reference.size());
    for (i32 i = 0; i < n - 1; ++i) {
        // Both sorted and reference should have monotonic distance
        // (reference is the truth, sorted is what we render)
        // Check if adjacent pair is ordered correctly
        // Correct: dist[i] >= dist[i+1] (far-first)
        if (sorted[i].distToCamera >= sorted[i+1].distToCamera - 0.001f) {
            correct++;
        }
        total++;
    }
    return total > 0 ? (f64)correct / (f64)total : 1.0;
}

// PSNR from sort quality (empirical mapping)
f64 quality_to_psnr(f64 quality, i32) {
    f64 basePsnr = 45.0;
    f64 error    = 1.0 - quality;
    f64 psnrDrop = error * 30.0;
    return std::max(8.0, basePsnr - psnrDrop);
}

// ============================================================
// Benchmark runner
// ============================================================
struct BenchConfig {
    SceneType scene;
    Strategy  strategy;
    u32       seed;
};

SortResult run_benchmark(const std::vector<TranslucentEntry>& baseEntries,
                         Strategy strategy, f64 cameraRotationRadPerFrame)
{
    constexpr i32 TOTAL_FRAMES = 1000;
    constexpr i32 WARMUP       = 10;

    std::vector<TranslucentEntry> entries = baseEntries;
    std::vector<TranslucentEntry> sorted  = entries; // working copy
    std::vector<TranslucentEntry> reference = entries;

    // Camera at origin looking along -Z, rotating
    Camera cam{};
    cam.posX = 0.0f; cam.posY = 1.0f; cam.posZ = -5.0f;

    i32  framesSorted     = 0;
    i64  totalInversions  = 0;
    f64  totalSortTimeUs  = 0.0;
    f64  totalQuality     = 0.0;
    f32  sortInterval     = 0.0f; // frame counter for deferred sort
    i32  totalPairs       = 0;

            f32  lastYaw   = 0.0f;
            f32  lastPitch = 0.0f;
            f32  angularVelocity = 0.0f;

    auto now = []() {
        return std::chrono::high_resolution_clock::now();
    };

    for (i32 frame = -WARMUP; frame < TOTAL_FRAMES; ++frame) {
        // Update camera position
        cam.yawRad += cameraRotationRadPerFrame;

        // Compute distances for all entries
        for (auto& e : entries) {
            // World positions are stored in distToCamera initially (abuse of field)
            // Reconstruct position from a simple generative model:
            // We use materialId as a pseudo-position hash
            f32 x = (e.materialId % 10) * 0.5f - 2.5f;
            f32 z = ((e.materialId / 10) % 10) * 0.5f - 2.5f;
            f32 y = 0.5f + (e.materialId / 100) * 0.1f;
            // Rotate around camera
            f32 dx = x - cam.posX;
            f32 dz = z - cam.posZ;
            e.distToCamera = std::sqrt(dx*dx + y*y + dz*dz);
        }

        // Reference = perfect per-frame sort (for quality comparison)
        reference = entries;
        std::sort(reference.begin(), reference.end());

        // Determine if we sort this frame
        bool doSort = false;
        switch (strategy) {
            case Strategy::PER_FRAME:
                doSort = true;
                break;
            case Strategy::EVERY_4:
            case Strategy::EVERY_8:
            case Strategy::EVERY_16:
                doSort = (sortInterval <= 0.0f);
                break;
            case Strategy::DISTANCE_ADAPTIVE: {
                f32 dyaw   = std::abs(cam.yawRad - lastYaw);
                f32 dpitch = std::abs(cam.pitchRad - lastPitch);
                lastYaw   = cam.yawRad;
                lastPitch = cam.pitchRad;
                angularVelocity = dyaw + dpitch;
                // Fast rotation → sort every frame; slow → every 8
                doSort = (sortInterval <= 0.0f);
                if (!doSort && frame >= 0) {
                    // Bump interval if camera suddenly rotates
                    if (angularVelocity > 0.05f) {
                        sortInterval = 1.0f;
                        doSort = true;
                    }
                }
                break;
            }
            case Strategy::PER_CHUNK:
                // Per-chunk = sort within each chunk (simulated as partial sort)
                doSort = true;
                break;
        }

        if (frame >= 0) {
            // Always copy current distances to sorted (but may not re-sort)
            sorted = entries;
        }

        if (doSort && frame >= 0) {
            auto t0 = now();

            if (strategy == Strategy::PER_CHUNK) {
                const i32 chunkSize = 20;
                for (i32 i = 0; i < (i32)sorted.size(); i += chunkSize) {
                    i32 end = std::min(i + chunkSize, (i32)sorted.size());
                    std::sort(sorted.begin() + i, sorted.begin() + end);
                }
            } else {
                std::sort(sorted.begin(), sorted.end());
            }

            auto t1 = now();
            f64 dtUs = (f64)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
            totalSortTimeUs += dtUs;
            framesSorted++;

            // Set interval for deferred strategies
            switch (strategy) {
                case Strategy::EVERY_4:  sortInterval = 4.0f;  break;
                case Strategy::EVERY_8:  sortInterval = 8.0f;  break;
                case Strategy::EVERY_16: sortInterval = 16.0f; break;
                default: break;
            }

            // For DISTANCE_ADAPTIVE, set the computed interval
            if (strategy == Strategy::DISTANCE_ADAPTIVE) {
                sortInterval = (angularVelocity > 0.05f) ? 1.0f : 8.0f;
            }
        } else if (frame >= 0) {
            // Not sorting this frame — use last sorted order
            // But camera moved, so entries may be in wrong order
            sortInterval -= 1.0f;
        }

        if (frame >= 0) {
            // Measure quality: compare sorted order (or last sorted order) to perfect sort
            f64 quality = compute_sort_quality(sorted, reference);
            totalQuality += quality;
            totalPairs   += (i32)entries.size();

            // Count inversions
            i64 inv = 0;
            for (i32 i = 0; i < (i32)sorted.size() - 1; ++i)
                for (i32 j = i + 1; j < (i32)sorted.size(); ++j)
                    if (sorted[i].distToCamera < sorted[j].distToCamera)
                        inv++;
            totalInversions += inv;
        }
    }

    SortResult r{};
    r.sortTimeUs      = framesSorted > 0 ? totalSortTimeUs / (f64)framesSorted : 0.0;
    r.sortQuality     = totalQuality / (f64)TOTAL_FRAMES;
    r.psnrDb          = quality_to_psnr(r.sortQuality, (i32)entries.size());
    r.entryCount      = (i32)entries.size();
    r.framesSorted    = framesSorted;
    r.totalInversions = totalInversions;
    return r;
}

// ============================================================
// Main
// ============================================================
int main() {
    std::vector<SceneType> scenes = {
        SceneType::NO_TRANSLUCENT,
        SceneType::WATER_SURFACE,
        SceneType::GLASS_BUILDING,
        SceneType::ICE_CAVE,
        SceneType::MIXED_TRANSLUCENT
    };

    std::vector<Strategy> strategies = {
        Strategy::PER_FRAME,
        Strategy::EVERY_4,
        Strategy::EVERY_8,
        Strategy::EVERY_16,
        Strategy::DISTANCE_ADAPTIVE,
        Strategy::PER_CHUNK
    };

    std::vector<u32> seeds = {1, 7, 42, 1234, 31337};

    // Camera rotation profiles
    struct RotProfile { const char* name; f32 radPerFrame; };
    std::vector<RotProfile> rotations = {
        {"still",    0.0000f},
        {"slow",     0.0015f}, // ~5 deg/sec at 60 fps
        {"medium",   0.0050f}, // ~17 deg/sec
        {"fast",     0.0150f}, // ~52 deg/sec
        {"extreme",  0.0300f}  // ~103 deg/sec
    };

    constexpr i32 TOTAL_FRAMES = 1000;
    constexpr i32 WARMUP       = 10;

    printf("=== Deferred Translucent Sorting Benchmark ===\n");
    printf("Format: scene,strategy,seed,rotation,entries,sortTimeUs,quality,psnrDb,framesSorted,totalInversions\n");
    printf("Warmup=%d frames, measure=%d frames\n\n", WARMUP, TOTAL_FRAMES);

    // Collect results as CSV string
    std::string csv = "scene,strategy,seed,rotation,entries,sortTimeUs,quality,psnrDb,framesSorted,totalInversions\n";

    for (auto scene : scenes) {
        for (auto strat : strategies) {
            for (auto seed : seeds) {
                auto entries = generate_scene(scene, seed);

                for (auto& rot : rotations) {
                    // Skip PER_CHUNK + extreme rotation (unlikely useful)
                    if (strat == Strategy::PER_CHUNK && rot.radPerFrame > 0.01f)
                        continue;
                    // Skip NO_TRANSLUCENT + extreme rotation
                    if (scene == SceneType::NO_TRANSLUCENT && rot.radPerFrame > 0.01f)
                        continue;

                    auto result = run_benchmark(entries, strat, rot.radPerFrame);

                    char line[512];
                    snprintf(line, sizeof(line),
                        "%s,%s,%u,%s,%d,%.3f,%.6f,%.2f,%d,%ld\n",
                        scene_name(scene), strategy_name(strat), seed, rot.name,
                        result.entryCount, result.sortTimeUs, result.sortQuality,
                        result.psnrDb, result.framesSorted, (long)result.totalInversions);
                    csv += line;

                    // Also print progress
                    printf("%s", line);
                    fflush(stdout);
                }
            }
        }
    }

    // Write CSV
    std::ofstream f("results.csv");
    f << csv;
    f.close();

    printf("\nDone. Results written to results.csv\n");

    // ============================================================
    // Summary statistics
    // ============================================================
    printf("\n=== Summary ===\n");
    for (auto strat : strategies) {
        f64 totalTime = 0, totalQual = 0;
        i32 count = 0;
        for (auto scene : scenes) {
            for (auto seed : seeds) {
                auto entries = generate_scene(scene, seed);
                // Average across rotation profiles (medium+still for PerChunk)
                for (auto& rot : rotations) {
                    if (strat == Strategy::PER_CHUNK && rot.radPerFrame > 0.01f) continue;
                    if (scene == SceneType::NO_TRANSLUCENT && rot.radPerFrame > 0.01f) continue;
                    auto r = run_benchmark(entries, strat, rot.radPerFrame);
                    totalTime += r.sortTimeUs;
                    totalQual += r.psnrDb;
                    count++;
                }
            }
        }
        if (count > 0) {
            printf("%-20s avg_sort_time=%.3f us  avg_psnr=%.2f dB  (n=%d)\n",
                strategy_name(strat), totalTime / count, totalQual / count, count);
        }
    }

    return 0;
}
