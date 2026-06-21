// 2026-06-21-vk-multi-gpu-split-frame — API discovery harness (Phase 1)
// Standalone Vulkan 1.4 + C++26 + volk — single file, no other deps.
// NOT ProjectV mainline. Per `AGENTS.md §1`: agent not building — operator can build/run.
//
// Probe:
//   - vkEnumeratePhysicalDeviceGroupsKHR → device group count + per-group physical device count + subset allocation
//   - vkGetDeviceGroupPresentCapabilitiesKHR → present modes per device
//   - vkGetDeviceGroupPeerMemoryFeaturesKHR → peer memory feature flags per device pair
// Output: build/api_discovery.json (machine-readable) + stdout (human-readable)

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

// Minimal volk loader (vendored at external/volk/, header-only or single TU)
#include <volk.h>

namespace mgpu_api {

struct PeerMemoryResult {
    uint32_t src;
    uint32_t dst;
    VkPeerMemoryFeatureFlags flags;
};

struct ApiDiscovery {
    uint32_t device_group_count = 0;
    std::vector<uint32_t> physical_device_count_per_group;
    std::vector<VkBool32> subset_allocation_per_group;
    // Present capabilities (per device, per present mode)
    std::vector<VkDeviceGroupPresentCapabilitiesKHR> present_caps;
    // Peer memory features (per pair)
    std::vector<PeerMemoryResult> peer_memory;
};

bool probe(VkInstance instance, VkPhysicalDevice primary, ApiDiscovery& out) {
    // 1. Enumerate device groups
    uint32_t group_count = 0;
    VkResult r = vkEnumeratePhysicalDeviceGroups(instance, &group_count, nullptr);
    if (r != VK_SUCCESS || group_count == 0) {
        std::fprintf(stderr, "ERROR: vkEnumeratePhysicalDeviceGroups returned %d (group_count=%u)\n", r, group_count);
        return false;
    }
    out.device_group_count = group_count;
    std::vector<VkPhysicalDeviceGroupProperties> groups(group_count);
    for (auto& g : groups) g.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    r = vkEnumeratePhysicalDeviceGroups(instance, &group_count, groups.data());
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "ERROR: vkEnumeratePhysicalDeviceGroups fill returned %d\n", r);
        return false;
    }
    for (const auto& g : groups) {
        out.physical_device_count_per_group.push_back(g.physicalDeviceCount);
        out.subset_allocation_per_group.push_back(g.subsetAllocation);
    }

    // 2. Present capabilities (per device in primary group)
    if (!groups.empty() && groups[0].physicalDeviceCount > 0) {
        VkDeviceGroupPresentCapabilitiesKHR caps{};
        caps.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR;
        r = vkGetDeviceGroupPresentCapabilitiesKHR(primary, &caps);
        if (r == VK_SUCCESS) {
            out.present_caps.push_back(caps);
        }
    }

    // 3. Peer memory features (per device pair within first group)
    if (!groups.empty() && groups[0].physicalDeviceCount > 1) {
        const uint32_t n = groups[0].physicalDeviceCount;
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = 0; j < n; ++j) {
                if (i == j) continue;
                VkPeerMemoryFeatureFlags flags = 0;
                vkGetDeviceGroupPeerMemoryFeaturesKHR(groups[0].physicalDevices[i], groups[0].physicalDevices[j],
                                                    groups[0].physicalDevices[i] /* heapIndex, simplified */, &flags);
                out.peer_memory.push_back({i, j, flags});
            }
        }
    }
    return true;
}

void write_json(const ApiDiscovery& d, const std::string& path) {
    std::ofstream f(path);
    if (!f) return;
    f << "{\n";
    f << "  \"device_group_count\": " << d.device_group_count << ",\n";
    f << "  \"groups\": [\n";
    for (size_t i = 0; i < d.physical_device_count_per_group.size(); ++i) {
        f << "    {\n";
        f << "      \"physical_device_count\": " << d.physical_device_count_per_group[i] << ",\n";
        f << "      \"subset_allocation\": " << (d.subset_allocation_per_group[i] ? "true" : "false") << "\n";
        f << "    }" << (i + 1 < d.physical_device_count_per_group.size() ? "," : "") << "\n";
    }
    f << "  ],\n";
    f << "  \"present_capabilities\": [\n";
    for (size_t i = 0; i < d.present_caps.size(); ++i) {
        const auto& c = d.present_caps[i];
        f << "    {\n";
        f << "      \"modes\": [\n";
        f << "        {\"name\": \"LOCAL\", \"supported\": " << ((c.modes & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR) ? "true" : "false") << "},\n";
        f << "        {\"name\": \"REMOTE\", \"supported\": " << ((c.modes & VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR) ? "true" : "false") << "},\n";
        f << "        {\"name\": \"SUM_SFR\", \"supported\": " << ((c.modes & VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR) ? "true" : "false") << "},\n";
        f << "        {\"name\": \"LOCAL_MULTI_DEVICE_AFR\", \"supported\": " << ((c.modes & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR) ? "true" : "false") << "}\n";
        f << "      ]\n";
        f << "    }" << (i + 1 < d.present_caps.size() ? "," : "") << "\n";
    }
    f << "  ],\n";
    f << "  \"peer_memory_features\": [\n";
    for (size_t i = 0; i < d.peer_memory.size(); ++i) {
        const auto& p = d.peer_memory[i];
        f << "    {\"src\": " << p.src << ", \"dst\": " << p.dst
          << ", \"COPY_DST\": " << ((p.flags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) ? "true" : "false")
          << ", \"COPY_SRC\": " << ((p.flags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) ? "true" : "false")
          << ", \"GENERIC_DST\": " << ((p.flags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) ? "true" : "false")
          << ", \"GENERIC_SRC\": " << ((p.flags & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT) ? "true" : "false")
          << "}" << (i + 1 < d.peer_memory.size() ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";
    f.close();
}

}  // namespace mgpu_api

int main(int argc, char** argv) {
    std::string output_path = "api_discovery.json";
    if (argc > 2 && std::string(argv[1]) == "--output") output_path = argv[2];

    // Vulkan init (minimal — no surface, no device)
    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "ERROR: volkInitialize failed\n");
        return 1;
    }
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "mgpu-api-discovery";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "ProjectV-Research";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;
    VkInstanceCreateInfo inst_info{};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo = &app_info;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&inst_info, nullptr, &instance);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "ERROR: vkCreateInstance returned %d (Vulkan 1.4 instance required)\n", r);
        return 1;
    }
    volkLoadInstance(instance);

    // Pick first physical device (for present caps probe)
    uint32_t pd_count = 0;
    vkEnumeratePhysicalDevices(instance, &pd_count, nullptr);
    if (pd_count == 0) {
        std::fprintf(stderr, "ERROR: no Vulkan-capable physical devices\n");
        vkDestroyInstance(instance, nullptr);
        return 1;
    }
    std::vector<VkPhysicalDevice> pds(pd_count);
    vkEnumeratePhysicalDevices(instance, &pd_count, pds.data());
    VkPhysicalDevice primary = pds[0];

    mgpu_api::ApiDiscovery d;
    bool ok = mgpu_api::probe(instance, primary, d);
    mgpu_api::write_json(d, output_path);

    // Human-readable summary
    std::printf("=== Multi-GPU API Discovery (dev host `obvium` per hardware-profile.md) ===\n");
    std::printf("device_group_count: %u\n", d.device_group_count);
    for (uint32_t i = 0; i < d.device_group_count; ++i) {
        std::printf("  group[%u]: physicalDeviceCount=%u, subsetAllocation=%s\n",
                    i, d.physical_device_count_per_group[i],
                    d.subset_allocation_per_group[i] ? "true" : "false");
    }
    if (d.peer_memory.empty()) {
        std::printf("peer_memory_features: (none — single-GPU logical device)\n");
    } else {
        std::printf("peer_memory_features: %zu pairs\n", d.peer_memory.size());
        for (const auto& p : d.peer_memory) {
            std::printf("  src=%u dst=%u flags=0x%x (COPY_DST=%d COPY_SRC=%d GENERIC_DST=%d GENERIC_SRC=%d)\n",
                        p.src, p.dst, p.flags,
                        (int)(p.flags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT),
                        (int)(p.flags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT),
                        (int)(p.flags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT),
                        (int)(p.flags & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT));
        }
    }
    for (size_t i = 0; i < d.present_caps.size(); ++i) {
        const auto& c = d.present_caps[i];
        std::printf("present_capabilities[%zu]: modes=0x%x (LOCAL=%d REMOTE=%d SUM_SFR=%d LOCAL_MULTI_DEVICE_AFR=%d)\n",
                    i, c.modes,
                    (int)!!(c.modes & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR),
                    (int)!!(c.modes & VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR),
                    (int)!!(c.modes & VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR),
                    (int)!!(c.modes & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR));
    }
    std::printf("OK: wrote %s (success=%d)\n", output_path.c_str(), ok);

    vkDestroyInstance(instance, nullptr);
    return ok ? 0 : 1;
}
