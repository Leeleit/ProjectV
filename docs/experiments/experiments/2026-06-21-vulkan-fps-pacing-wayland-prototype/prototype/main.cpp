#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "triangle.vert.spv.h"
#include "triangle.frag.spv.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#define CHECK_VK(expr)                                                            \
    do {                                                                          \
        VkResult _r = (expr);                                                     \
        if (_r != VK_SUCCESS && _r != VK_SUBOPTIMAL_KHR && _r != VK_ERROR_OUT_OF_DATE_KHR) { \
            std::fprintf(stderr, "[VK ERROR] %s = %d at %s:%d\n", #expr, (int)_r, \
                         __FILE__, __LINE__);                                     \
            std::exit(1);                                                         \
        }                                                                         \
    } while (0)

namespace {

constexpr uint32_t kWindowWidth = 640;
constexpr uint32_t kWindowHeight = 360;
constexpr uint32_t kFramesPerScenario = 100;
constexpr uint32_t kWarmupFrames = 5;
constexpr uint32_t kSeeds[] = {1, 7, 42, 1234, 31337};
constexpr uint32_t kNumSeeds = sizeof(kSeeds) / sizeof(kSeeds[0]);

const char* kModeNames[] = {"A", "B", "C", "D", "E"};
const char* kScenarioNames[] = {"cpu_bound", "gpu_bound", "jitter"};

enum class Mode {
    A_BusyWaitFifo = 0,
    B_FifoLatestReady = 1,
    C_PresentWait2 = 2,
    D_PresentTiming = 3,
    E_PresentTimingPlusFifoLatestReady = 4,
    Count
};

enum class Scenario {
    CpuBound = 0,
    GpuBound = 1,
    Jitter = 2,
    Count
};

struct App {
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;

    uint32_t graphics_family = 0;

    VkPhysicalDeviceProperties dev_props{};

    bool has_present_timing = false;
    bool has_present_at_absolute = false;
    bool has_present_wait2 = false;
    bool has_swapchain_maint1 = false;
    bool has_fifo_latest_ready = false;

    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_views;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipe = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE;
    VkShaderModule fs = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> cmds;
    VkFence present_fence = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;

    std::ofstream csv;

    uint64_t refresh_duration_ns = 16'666'667ULL;
};

const char* kVertexShaderSrc = R"(
#version 450
vec2 positions[3] = vec2[](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));
void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
)";

const char* kFragmentShaderSrc = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.2, 0.4, 0.6, 1.0);
}
)";

uint64_t NowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

bool HasExt(const std::vector<VkExtensionProperties>& exts, const char* name) {
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

void PickMode(Mode mode, App& app) {
    switch (mode) {
        case Mode::A_BusyWaitFifo:
            app.selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        case Mode::B_FifoLatestReady:
            app.selected_present_mode = app.has_fifo_latest_ready
                                            ? VK_PRESENT_MODE_FIFO_LATEST_READY_KHR
                                            : VK_PRESENT_MODE_FIFO_KHR;
            break;
        case Mode::C_PresentWait2:
            app.selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        case Mode::D_PresentTiming:
            app.selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        case Mode::E_PresentTimingPlusFifoLatestReady:
            app.selected_present_mode = app.has_fifo_latest_ready
                                            ? VK_PRESENT_MODE_FIFO_LATEST_READY_KHR
                                            : VK_PRESENT_MODE_FIFO_KHR;
            break;
        default:
            app.selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;
    }
}

void CreateSwapchain(App& app) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.phys, app.surface, &caps);

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.phys, app.surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.phys, app.surface, &fmt_count, fmts.data());
    VkSurfaceFormatKHR fmt = fmts[0];
    for (const auto& f : fmts) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = f;
            break;
        }
    }

    uint32_t pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(app.phys, app.surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> pms(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(app.phys, app.surface, &pm_count, pms.data());
    bool pm_available = false;
    for (auto pm : pms) {
        if (pm == app.selected_present_mode) {
            pm_available = true;
            break;
        }
    }
    if (!pm_available) {
        std::fprintf(stderr, "[WARN] present mode %d not available, falling back to FIFO\n",
                     (int)app.selected_present_mode);
        app.selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = app.surface;
    sci.minImageCount = caps.minImageCount + 1;
    sci.imageFormat = fmt.format;
    sci.imageColorSpace = fmt.colorSpace;
    sci.imageExtent = {kWindowWidth, kWindowHeight};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = app.selected_present_mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    CHECK_VK(vkCreateSwapchainKHR(app.device, &sci, nullptr, &app.swapchain));

    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(app.device, app.swapchain, &actual_count, nullptr);
    app.swapchain_images.resize(actual_count);
    vkGetSwapchainImagesKHR(app.device, app.swapchain, &actual_count, app.swapchain_images.data());

    app.swapchain_views.resize(actual_count);
    for (uint32_t i = 0; i < actual_count; ++i) {
        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = app.swapchain_images[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = fmt.format;
        ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        CHECK_VK(vkCreateImageView(app.device, &ivci, nullptr, &app.swapchain_views[i]));
    }

    if (app.has_present_timing) {
        PFN_vkGetSwapchainTimingPropertiesEXT get_timing =
            (PFN_vkGetSwapchainTimingPropertiesEXT)vkGetDeviceProcAddr(
                app.device, "vkGetSwapchainTimingPropertiesEXT");
        if (get_timing) {
            VkSwapchainTimingPropertiesEXT tp{VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT};
            uint64_t counter = 0;
            get_timing(app.device, app.swapchain, &tp, &counter);
            if (counter == 0) {
                get_timing(app.device, app.swapchain, &tp, &counter);
            }
            if (counter > 0 && tp.refreshDuration > 0) {
                app.refresh_duration_ns = tp.refreshDuration;
                std::fprintf(stderr, "[INFO] refreshDuration = %lu ns (%.3f ms)\n",
                             (unsigned long)app.refresh_duration_ns,
                             app.refresh_duration_ns / 1e6);
            } else {
                std::fprintf(stderr, "[INFO] no swapchain timing properties, using default\n");
            }
        }
    }
}

void CreateRenderPass(App& app) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_B8G8R8A8_SRGB;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;

    CHECK_VK(vkCreateRenderPass(app.device, &rpci, nullptr, &app.render_pass));

    app.framebuffers.resize(app.swapchain_views.size());
    for (size_t i = 0; i < app.swapchain_views.size(); ++i) {
        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = app.render_pass;
        fci.attachmentCount = 1;
        fci.pAttachments = &app.swapchain_views[i];
        fci.width = kWindowWidth;
        fci.height = kWindowHeight;
        fci.layers = 1;
        CHECK_VK(vkCreateFramebuffer(app.device, &fci, nullptr, &app.framebuffers[i]));
    }
}

VkShaderModule CreateShader(App& app, const unsigned char* code, size_t size) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = size;
    ci.pCode = reinterpret_cast<const uint32_t*>(code);
    VkShaderModule mod;
    CHECK_VK(vkCreateShaderModule(app.device, &ci, nullptr, &mod));
    return mod;
}

void CreatePipeline(App& app) {
    app.vs = CreateShader(app, triangle_vert_spv, triangle_vert_spv_len);
    app.fs = CreateShader(app, triangle_frag_spv, triangle_frag_spv_len);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = app.vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = app.fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynci{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynci.dynamicStateCount = 2;
    dynci.pDynamicStates = dyn;

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    CHECK_VK(vkCreatePipelineLayout(app.device, &plci, nullptr, &app.pipe_layout));

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dynci;
    gpci.layout = app.pipe_layout;
    gpci.renderPass = app.render_pass;
    gpci.subpass = 0;

    CHECK_VK(vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &gpci, nullptr,
                                       &app.graphics_pipe));
}

void InitApp(App& app) {
    SDL_Init(SDL_INIT_VIDEO);
    app.window = SDL_CreateWindow("frame_pacing_bench", kWindowWidth, kWindowHeight,
                                  SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);

    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "frame_pacing_bench";
    ai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.pEngineName = "pacing";
    ai.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.apiVersion = VK_API_VERSION_1_4;

    std::vector<const char*> instance_exts;
    uint32_t sdl_ext_count = 0;
    const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
    for (uint32_t i = 0; i < sdl_ext_count; ++i) instance_exts.push_back(sdl_exts[i]);

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    ici.enabledExtensionCount = (uint32_t)instance_exts.size();
    ici.ppEnabledExtensionNames = instance_exts.data();
    CHECK_VK(vkCreateInstance(&ici, nullptr, &app.instance));

    SDL_Vulkan_CreateSurface(app.window, app.instance, nullptr, &app.surface);

    uint32_t dev_count = 0;
    vkEnumeratePhysicalDevices(app.instance, &dev_count, nullptr);
    std::vector<VkPhysicalDevice> devs(dev_count);
    vkEnumeratePhysicalDevices(app.instance, &dev_count, devs.data());
    app.phys = devs[0];

    vkGetPhysicalDeviceProperties(app.phys, &app.dev_props);

    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app.phys, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(app.phys, &qf_count, qfs.data());
    app.graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; ++i) {
        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(app.phys, i, app.surface, &present_support);
        if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
            app.graphics_family = i;
            break;
        }
    }
    if (app.graphics_family == UINT32_MAX) {
        std::fprintf(stderr, "[FATAL] no graphics+present queue found\n");
        std::exit(2);
    }

    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(app.phys, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> dev_exts(ext_count);
    vkEnumerateDeviceExtensionProperties(app.phys, nullptr, &ext_count, dev_exts.data());
    app.has_present_timing = HasExt(dev_exts, "VK_EXT_present_timing");
    app.has_present_wait2 = HasExt(dev_exts, "VK_KHR_present_wait2");
    app.has_swapchain_maint1 = HasExt(dev_exts, "VK_KHR_swapchain_maintenance1");
    app.has_fifo_latest_ready = HasExt(dev_exts, "VK_KHR_present_mode_fifo_latest_ready");

    VkPhysicalDevicePresentTimingFeaturesEXT pt_feat{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT};
    VkPhysicalDevicePresentWait2FeaturesKHR pw2_feat{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR};
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR sm1_feat{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR};
    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR flr_feat{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR};

    void* chain_head = nullptr;
    void* chain_tail = nullptr;
    auto append = [&](void* p) {
        if (!chain_head) {
            chain_head = p;
            chain_tail = p;
        } else {
            *(void**)chain_tail = p;
            chain_tail = p;
        }
    };

    if (app.has_present_timing) {
        pt_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT;
        pt_feat.presentTiming = VK_TRUE;
        pt_feat.presentAtAbsoluteTime = VK_TRUE;
        pt_feat.presentAtRelativeTime = VK_FALSE;
        append(&pt_feat);
    }
    if (app.has_present_wait2) {
        pw2_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR;
        pw2_feat.presentWait2 = VK_TRUE;
        append(&pw2_feat);
    }
    if (app.has_swapchain_maint1) {
        sm1_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
        sm1_feat.swapchainMaintenance1 = VK_TRUE;
        append(&sm1_feat);
    }
    if (app.has_fifo_latest_ready) {
        flr_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR;
        flr_feat.presentModeFifoLatestReady = VK_TRUE;
        append(&flr_feat);
    }

    std::vector<const char*> enabled_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (app.has_present_timing) enabled_exts.push_back("VK_EXT_present_timing");
    if (app.has_present_wait2) enabled_exts.push_back("VK_KHR_present_wait2");
    if (app.has_swapchain_maint1) enabled_exts.push_back("VK_KHR_swapchain_maintenance1");
    if (app.has_fifo_latest_ready) enabled_exts.push_back("VK_KHR_present_mode_fifo_latest_ready");

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = app.graphics_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = (uint32_t)enabled_exts.size();
    dci.ppEnabledExtensionNames = enabled_exts.data();
    dci.pNext = chain_head;
    CHECK_VK(vkCreateDevice(app.phys, &dci, nullptr, &app.device));

    vkGetDeviceQueue(app.device, app.graphics_family, 0, &app.graphics_queue);

    std::fprintf(stderr, "[INFO] extensions: present_timing=%d present_wait2=%d "
                          "swapchain_maint1=%d fifo_latest_ready=%d\n",
                 (int)app.has_present_timing, (int)app.has_present_wait2,
                 (int)app.has_swapchain_maint1, (int)app.has_fifo_latest_ready);

    PickMode(Mode::A_BusyWaitFifo, app);
    CreateSwapchain(app);
    CreateRenderPass(app);
    CreatePipeline(app);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = app.graphics_family;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK_VK(vkCreateCommandPool(app.device, &cpci, nullptr, &app.cmd_pool));

    app.cmds.resize(app.framebuffers.size());
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = app.cmd_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = (uint32_t)app.cmds.size();
    CHECK_VK(vkAllocateCommandBuffers(app.device, &cbai, app.cmds.data()));

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK_VK(vkCreateFence(app.device, &fci, nullptr, &app.present_fence));

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CHECK_VK(vkCreateSemaphore(app.device, &sci, nullptr, &app.image_available));
    CHECK_VK(vkCreateSemaphore(app.device, &sci, nullptr, &app.render_finished));

    std::fprintf(stderr, "[INFO] device=%s driver=%u.%u api=%u.%u\n",
                 app.dev_props.deviceName,
                 VK_VERSION_MAJOR(app.dev_props.driverVersion),
                 VK_VERSION_MINOR(app.dev_props.driverVersion),
                 VK_VERSION_MAJOR(app.dev_props.apiVersion),
                 VK_VERSION_MINOR(app.dev_props.apiVersion));
}

void RecordCmd(App& app, VkCommandBuffer cmd, VkFramebuffer fb) {
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK_VK(vkBeginCommandBuffer(cmd, &cbbi));

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = app.render_pass;
    rpbi.framebuffer = fb;
    rpbi.renderArea = {{0, 0}, {kWindowWidth, kWindowHeight}};
    VkClearValue cv{};
    cv.color = {{0.2f, 0.2f, 0.2f, 1.0f}};
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &cv;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{0, 0, (float)kWindowWidth, (float)kWindowHeight, 0, 1};
    VkRect2D sc{{0, 0}, {kWindowWidth, kWindowHeight}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphics_pipe);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    CHECK_VK(vkEndCommandBuffer(cmd));
}

void CpuSleepUs(uint64_t us) {
    auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
    while (std::chrono::steady_clock::now() < end) {
    }
}

struct FrameTimings {
    double cpu_acquire_us;
    double cpu_present_us;
    double gpu_simulated_us;
    double frame_interval_us;
    double target_time_offset_us;
    uint64_t target_time_ns;
};

FrameTimings RunFrame(App& app, Mode mode, Scenario scenario, uint32_t frame_idx) {
    FrameTimings t{};
    uint64_t frame_start = NowNs();

    uint32_t image_idx = 0;
    VkResult r = vkAcquireNextImageKHR(app.device, app.swapchain, UINT64_MAX,
                                       app.image_available, VK_NULL_HANDLE, &image_idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        return t;
    }
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "[VK] acquire failed: %d\n", (int)r);
        return t;
    }

    uint64_t after_acquire = NowNs();
    t.cpu_acquire_us = (after_acquire - frame_start) / 1e3;

    if (scenario == Scenario::CpuBound) {
        CpuSleepUs(100);
        t.gpu_simulated_us = 100;
    } else if (scenario == Scenario::GpuBound) {
        CpuSleepUs(1000);
        t.gpu_simulated_us = 1000;
    } else {
        uint64_t sim_us = (frame_idx & 1) ? 500 : 1500;
        CpuSleepUs(sim_us);
        t.gpu_simulated_us = (double)sim_us;
    }

    RecordCmd(app, app.cmds[image_idx], app.framebuffers[image_idx]);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &app.image_available;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &app.cmds[image_idx];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &app.render_finished;
    CHECK_VK(vkQueueSubmit(app.graphics_queue, 1, &si, app.present_fence));

    uint64_t after_submit = NowNs();

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &app.render_finished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &app.swapchain;
    pi.pImageIndices = &image_idx;

    uint64_t present_id_val = frame_idx + 1;
    VkPresentIdKHR present_id{VK_STRUCTURE_TYPE_PRESENT_ID_KHR};
    present_id.swapchainCount = 1;
    present_id.pPresentIds = &present_id_val;

    VkPresentTimingInfoEXT pti{VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT};
    pti.flags = 0;
    pti.targetTime = 0;
    pti.timeDomainId = 0;
    pti.presentStageQueries = 0;
    pti.targetTimeDomainPresentStage = 0;

    if (mode == Mode::D_PresentTiming || mode == Mode::E_PresentTimingPlusFifoLatestReady) {
        if (app.has_present_timing) {
            pti.targetTime = frame_start + app.refresh_duration_ns;
            t.target_time_ns = pti.targetTime;
            pi.pNext = &pti;
            if (app.has_present_wait2) {
                pti.pNext = &present_id;
            }
        } else if (app.has_present_wait2) {
            pi.pNext = &present_id;
        }
    } else if (mode == Mode::C_PresentWait2) {
        if (app.has_present_wait2) {
            pi.pNext = &present_id;
        }
    }

    uint64_t before_present = NowNs();
    VkResult pr = vkQueuePresentKHR(app.graphics_queue, &pi);
    uint64_t after_present = NowNs();
    t.cpu_present_us = (after_present - before_present) / 1e3;

    if (mode == Mode::A_BusyWaitFifo) {
        uint64_t wait_deadline = after_present + 9'000'000ULL;
        while (NowNs() < wait_deadline) {
            VkResult wr = vkWaitForFences(app.device, 1, &app.present_fence, VK_TRUE, 1'000'000ULL);
            if (wr == VK_SUCCESS) break;
        }
    } else if (mode == Mode::C_PresentWait2 || mode == Mode::D_PresentTiming ||
               mode == Mode::E_PresentTimingPlusFifoLatestReady) {
        if (app.has_present_wait2 && (pr == VK_SUCCESS || pr == VK_SUBOPTIMAL_KHR)) {
            PFN_vkWaitForPresent2KHR wait_p2 =
                (PFN_vkWaitForPresent2KHR)vkGetDeviceProcAddr(app.device, "vkWaitForPresent2KHR");
            if (wait_p2) {
                VkPresentWait2InfoKHR pw2i{VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR};
                pw2i.presentId = present_id_val;
                pw2i.timeout = 10'000'000ULL;
                wait_p2(app.device, app.swapchain, &pw2i);
            }
        }
        vkWaitForFences(app.device, 1, &app.present_fence, VK_TRUE, UINT64_MAX);
    } else if (mode == Mode::B_FifoLatestReady) {
        vkWaitForFences(app.device, 1, &app.present_fence, VK_TRUE, UINT64_MAX);
    }

    vkResetFences(app.device, 1, &app.present_fence);

    uint64_t after_wait = NowNs();
    t.frame_interval_us = (after_wait - frame_start) / 1e3;

    if (t.target_time_ns > 0) {
        t.target_time_offset_us = ((double)after_present - (double)t.target_time_ns) / 1e3;
    }

    return t;
}

void RunMode(App& app, Mode mode, Scenario scenario, uint32_t seed) {
    std::fprintf(stderr, "[RUN] mode=%s scenario=%s seed=%u\n", kModeNames[(int)mode],
                 kScenarioNames[(int)scenario], seed);

    PickMode(mode, app);

    if (mode == Mode::B_FifoLatestReady || mode == Mode::E_PresentTimingPlusFifoLatestReady) {
        vkDeviceWaitIdle(app.device);
        vkDestroySwapchainKHR(app.device, app.swapchain, nullptr);
        for (auto v : app.swapchain_views) vkDestroyImageView(app.device, v, nullptr);
        for (auto fb : app.framebuffers) vkDestroyFramebuffer(app.device, fb, nullptr);
        vkDestroyRenderPass(app.device, app.render_pass, nullptr);
        app.swapchain_views.clear();
        app.framebuffers.clear();

        CreateSwapchain(app);
        CreateRenderPass(app);

        for (size_t i = 0; i < app.cmds.size(); ++i) {
            RecordCmd(app, app.cmds[i], app.framebuffers[i]);
        }
    }

    vkResetCommandPool(app.device, app.cmd_pool, 0);

    for (uint32_t i = 0; i < kWarmupFrames; ++i) {
        RunFrame(app, mode, scenario, i);
    }

    for (uint32_t i = 0; i < kFramesPerScenario; ++i) {
        FrameTimings t = RunFrame(app, mode, scenario, i);
        app.csv << kModeNames[(int)mode] << "," << kScenarioNames[(int)scenario] << "," << seed
                << "," << i << "," << t.cpu_acquire_us << "," << t.cpu_present_us << ","
                << t.gpu_simulated_us << "," << t.frame_interval_us << ","
                << t.target_time_offset_us << "," << t.target_time_ns << "\n";
    }
    app.csv.flush();
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App app;

    InitApp(app);

    app.csv.open("results.csv");
    app.csv << "mode,scenario,seed,frame_id,cpu_acquire_us,cpu_present_us,gpu_sim_us,"
               "frame_interval_us,target_offset_us,target_time_ns\n";

    for (int m = 0; m < (int)Mode::Count; ++m) {
        Mode mode = (Mode)m;
        if (mode == Mode::B_FifoLatestReady && !app.has_fifo_latest_ready) {
            std::fprintf(stderr, "[SKIP] mode %s — fifo_latest_ready unsupported\n",
                         kModeNames[m]);
            continue;
        }
        for (int s = 0; s < (int)Scenario::Count; ++s) {
            for (uint32_t seed : kSeeds) {
                RunMode(app, mode, (Scenario)s, seed);
            }
        }
    }

    app.csv.close();
    std::fprintf(stderr, "[DONE] results written to results.csv\n");
    return 0;
}
