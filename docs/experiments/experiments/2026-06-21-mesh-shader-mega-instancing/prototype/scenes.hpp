// SPDX-License-Identifier: MIT
// Scene configurations per README.md §3.
// 5 representative military-sandbox scenes spanning 1k to 1M instances.
#pragma once

#include <cstdint>
#include <string_view>

namespace sim {

struct Scene {
    std::string_view name;
    std::int32_t instance_count;
    std::int32_t material_count;     // 5-30 typical (infantry, vehicle, projectile per type)
    float hiz_occlusion_rate;        // 0.0-1.0 — fraction of instances HiZ-culled on average
    float frustum_cull_rate;         // 0.0-1.0 — fraction outside camera frustum on average
    float tris_per_meshlet;          // 64-128 typical (64-126 per Vulkanised 2023 + GameDev 2024)
    std::int32_t meshlets_per_instance;  // 1-4 typical (low-poly military unit)
    float world_extent_m;            // scene scale (for HiZ benefit estimate)
};

inline constexpr Scene kScenes[] = {
    // 1k scattered 1km x 1km — low density, no HiZ benefit, mostly frustum
    {
        .name = "scattered_1k",
        .instance_count = 1000,
        .material_count = 8,
        .hiz_occlusion_rate = 0.05f,
        .frustum_cull_rate = 0.30f,
        .tris_per_meshlet = 64.0f,
        .meshlets_per_instance = 2,
        .world_extent_m = 1000.0f,
    },
    // 10k packed 100m x 100m — high density, full HiZ benefit
    {
        .name = "dense_10k",
        .instance_count = 10'000,
        .material_count = 12,
        .hiz_occlusion_rate = 0.50f,
        .frustum_cull_rate = 0.20f,
        .tris_per_meshlet = 96.0f,
        .meshlets_per_instance = 2,
        .world_extent_m = 100.0f,
    },
    // 100k swarm 200m x 200m — RTT battle scale, 14% GPU util threshold
    {
        .name = "swarm_100k",
        .instance_count = 100'000,
        .material_count = 20,
        .hiz_occlusion_rate = 0.40f,
        .frustum_cull_rate = 0.25f,
        .tris_per_meshlet = 96.0f,
        .meshlets_per_instance = 1,
        .world_extent_m = 200.0f,
    },
    // 1M mega swarm 500m x 500m — military sandbox upper bound
    {
        .name = "mega_1m",
        .instance_count = 1'000'000,
        .material_count = 30,
        .hiz_occlusion_rate = 0.30f,
        .frustum_cull_rate = 0.30f,
        .tris_per_meshlet = 64.0f,
        .meshlets_per_instance = 1,
        .world_extent_m = 500.0f,
    },
    // 2k Total War formation 2 lines 100m x 1km — ~70% HiZ occlusion
    {
        .name = "frontline_2k",
        .instance_count = 2000,
        .material_count = 15,
        .hiz_occlusion_rate = 0.70f,
        .frustum_cull_rate = 0.15f,
        .tris_per_meshlet = 96.0f,
        .meshlets_per_instance = 2,
        .world_extent_m = 1000.0f,
    },
};

inline constexpr std::size_t kSceneCount = sizeof(kScenes) / sizeof(kScenes[0]);

}  // namespace sim
