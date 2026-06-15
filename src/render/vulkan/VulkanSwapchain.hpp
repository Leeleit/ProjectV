#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

#include "core/Types.hpp"

namespace projectv::swapchain {

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `CreateOrRecreateSwapchain`. Replaces the per-step
// `runtime::LogRuntimeFailure` + `return false` pattern with
// `std::expected<VkFormat, SwapchainError>`. Cold path (1× per
// swapchain recreate / window resize), so the `std::expected`
// cost is irrelevant. The error variant is the machine-readable
// signal; the per-step log line is preserved inside the
// implementation.
enum class SwapchainError : std::uint8_t {
	PreconditionFailed = 0,
	QuerySupportFailed,
	SurfaceUsageUnsupported,
	CreateSwapchainFailed,
	GetImageCountFailed,
	ZeroImages,
	GetImageListFailed,
	CreateImageViewFailed,
	CreateSemaphoreFailed,
};

constexpr std::string_view toString(SwapchainError e) noexcept {
	switch (e) {
	case SwapchainError::PreconditionFailed: return "PreconditionFailed";
	case SwapchainError::QuerySupportFailed: return "QuerySupportFailed";
	case SwapchainError::SurfaceUsageUnsupported: return "SurfaceUsageUnsupported";
	case SwapchainError::CreateSwapchainFailed: return "CreateSwapchainFailed";
	case SwapchainError::GetImageCountFailed: return "GetImageCountFailed";
	case SwapchainError::ZeroImages: return "ZeroImages";
	case SwapchainError::GetImageListFailed: return "GetImageListFailed";
	case SwapchainError::CreateImageViewFailed: return "CreateImageViewFailed";
	case SwapchainError::CreateSemaphoreFailed: return "CreateSemaphoreFailed";
	}
	return "Unknown";
}
} // namespace projectv::swapchain

// **Tier 1.B (`2026-06-13`).** Returns
// `std::expected<VkFormat, projectv::swapchain::SwapchainError>`.
// The chosen `VkFormat` is the new return value (was
// `swapchain->imageFormat` side effect). On success the format
// is BOTH returned and written to `swapchain->imageFormat` so
// existing call sites that read the format via the struct keep
// working. The higher-level `RecreateSwapchain` wrapper keeps
// its `bool` return — it adds TAA render-target recreation on
// top and is a different (per-frame) hot path that the
// std::expected refactor doesn't fit cleanly.
std::expected<VkFormat, projectv::swapchain::SwapchainError> CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain);

bool RecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render);

// **V-sync toggle hotkey (`2026-06-13`, auto-detect cycle
// on `2026-06-14`).** Walks the runtime present-mode cycle
// (built once per swapchain create from the surface's
// supported modes, see `BuildPresentModeCycle` below).
// Surfaces that don't expose IMMEDIATE (e.g. some Linux
// Wayland compositors) get a 2-mode cycle; surfaces that
// expose all three get a 3-mode cycle. The cycle is always
// physically meaningful — no "press V and nothing changes".
// Returns the *new* preferred mode so the caller can log it.
// The swapchain is *not* recreated from this call; the next
// frame's `ChoosePresentMode` picks up the new preference
// when the swapchain is rebuilt. For an immediate mode
// change, the caller should call `RecreateSwapchain` after
// invoking this; for the operator's `V` hotkey, the mode is
// just stashed here and picked up on the next natural
// recreate.
VkPresentModeKHR CyclePreferredPresentMode();

// **Build the runtime present-mode cycle (`2026-06-14`).**
// Called once per swapchain create, **after** the surface
// support query. Walks the priority list
// `{FIFO, MAILBOX, IMMEDIATE}` in order and keeps only the
// modes the surface exposes, producing a cycle vector
// `g_presentModeCycle`. Sets `g_preferredPresentMode` to the
// first (highest-priority supported) mode — typically FIFO
// for vsync-on as the default. Subsequent
// `CyclePreferredPresentMode` calls walk this vector. The
// returned vector is a copy for callers that want to
// inspect the cycle (e.g. tests); the engine stores its
// own copy internally.
//
// **Why not auto-detect the cycle from `vkGetPhysicalDevice-
// SurfaceSupport`-style queries?** `vkGetPhysicalDevice-
// SurfacePresentModes` is already called by
// `QuerySwapchainSupport`, and the result (`support.presentModes`)
// is the **authoritative** set of supported modes. We just
// need to **order** it by priority (FIFO first for vsync-
// on as default, then MAILBOX, then IMMEDIATE).
std::vector<VkPresentModeKHR> BuildPresentModeCycle(
	const std::vector<VkPresentModeKHR> &surfacePresentModes);

// **Read-only accessors for the HUD (`2026-06-14`).** The
// debug HUD shows `VSync: <mode> (<index>/<size>)` so the
// operator can see the current cycle state without parsing
// the log. Returns `VK_PRESENT_MODE_FIFO_KHR` if the cycle
// has never been built (e.g. headless test invocation).
//
// **Header-only (`2026-06-14`).** The cycle and the
// current mode are stored as `inline` C++17 variables in
// this header. `CyclePreferredPresentMode` and
// `BuildPresentModeCycle` are also `inline` so the entire
// present-mode API is test-target-friendly (no link
// against `VulkanSwapchain.cpp` required). The
// declarations previously lived in `VulkanSwapchain.cpp`;
// moving them here lets the `ProjectVPresentModeTests`
// test target exercise them without pulling in
// `VulkanGraphicsPipeline.cpp` / Tracy / etc. via
// `VulkanSwapchain.cpp`. Production `main.cpp` calls
// the same `inline` functions — the linker folds
// identical inlines.
namespace projectv::present_mode {
inline VkPresentModeKHR g_active = VK_PRESENT_MODE_FIFO_KHR;
inline std::vector<VkPresentModeKHR> g_cycle = {VK_PRESENT_MODE_FIFO_KHR};
} // namespace projectv::present_mode

inline VkPresentModeKHR GetActivePresentMode()
{
	return projectv::present_mode::g_active;
}

inline std::size_t GetPresentModeCycleSize()
{
	return projectv::present_mode::g_cycle.size();
}

inline std::size_t GetPresentModeCycleIndex(const VkPresentModeKHR mode)
{
	const auto it = std::find(
		projectv::present_mode::g_cycle.begin(),
		projectv::present_mode::g_cycle.end(),
		mode);
	if (it == projectv::present_mode::g_cycle.end()) {
		return 0u;
	}
	return static_cast<std::size_t>(it - projectv::present_mode::g_cycle.begin());
}

inline VkPresentModeKHR CyclePreferredPresentMode()
{
	// **V-sync toggle (auto-detect cycle on `2026-06-14`).**
	// Walks `g_cycle` (built once per swapchain create
	// from the surface's supported modes) one step
	// forward, wrapping at the end. See the long-form
	// comment block above for the full rationale.
	if (projectv::present_mode::g_cycle.size() <= 1) {
		return projectv::present_mode::g_active;
	}
	const auto it = std::find(
		projectv::present_mode::g_cycle.begin(),
		projectv::present_mode::g_cycle.end(),
		projectv::present_mode::g_active);
	const std::size_t currentIndex = (it == projectv::present_mode::g_cycle.end())
		? 0u
		: static_cast<std::size_t>(it - projectv::present_mode::g_cycle.begin());
	const std::size_t nextIndex = (currentIndex + 1u) % projectv::present_mode::g_cycle.size();
	projectv::present_mode::g_active = projectv::present_mode::g_cycle[nextIndex];
	return projectv::present_mode::g_active;
}

inline std::vector<VkPresentModeKHR> BuildPresentModeCycle(
	const std::vector<VkPresentModeKHR> &surfacePresentModes)
{
	// **Build the runtime present-mode cycle
	// (`2026-06-14`, fix on 2026-06-14 evening).**
	// Walks the priority list `{FIFO, MAILBOX, IMMEDIATE}`
	// in order and keeps only the modes the surface
	// exposes. The result is the cycle that
	// `CyclePreferredPresentMode` walks.
	//
	// **`g_active` preservation across rebuilds.**
	// The previous version unconditionally set
	// `g_active = g_cycle.front()` (FIFO) on every
	// rebuild. Combined with the V hotkey's
	// `RecreateSwapchain` call after each press, this
	// produced a stuck-in-place failure mode: V press
	// → `CyclePreferredPresentMode` advances to
	// `IMMEDIATE` → log "IMMEDIATE" → `RecreateSwapchain`
	// → `BuildPresentModeCycle` resets `g_active` to
	// `FIFO` → next V press sees `FIFO`, advances back
	// to `IMMEDIATE` → log "IMMEDIATE" again. Operator
	// reported: "нажимаю V, ничего не меняется" with
	// every log line showing `IMMEDIATE [cycle 2/2]`.
	//
	// **Fix:** capture the previous `g_active` before
	// rebuilding the cycle. If the previous mode is
	// still in the new cycle, keep it. Otherwise (display
	// hot-swap dropped the current mode), fall back to
	// the highest-priority supported mode. This way the
	// V hotkey walks the cycle correctly across
	// multiple swapchain recreates.
	static constexpr std::array<VkPresentModeKHR, 3> kPriority{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	const VkPresentModeKHR previousActive = projectv::present_mode::g_active;
	projectv::present_mode::g_cycle.clear();
	projectv::present_mode::g_cycle.reserve(kPriority.size());
	for (const VkPresentModeKHR mode : kPriority) {
		if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), mode)
			!= surfacePresentModes.end()) {
			projectv::present_mode::g_cycle.push_back(mode);
		}
	}
	if (projectv::present_mode::g_cycle.empty()) {
		// Vulkan 1.4 spec: `FIFO_KHR` is mandatory. If the
		// surface didn't report it, fall back to a single-
		// element cycle of FIFO so the engine still creates
		// a swapchain.
		projectv::present_mode::g_cycle.push_back(VK_PRESENT_MODE_FIFO_KHR);
	}
	// Preserve the previous `g_active` if it's still
	// supported; otherwise fall back to highest-priority.
	if (std::find(
			projectv::present_mode::g_cycle.begin(),
			projectv::present_mode::g_cycle.end(),
			previousActive)
		!= projectv::present_mode::g_cycle.end()) {
		projectv::present_mode::g_active = previousActive;
	} else {
		// Display hot-swap dropped the current mode
		// (e.g. external monitor unplugged and the new
		// surface doesn't expose IMMEDIATE). Fall back
		// to highest-priority supported mode, which is
		// FIFO on every conformant Vulkan 1.4 surface.
		projectv::present_mode::g_active = projectv::present_mode::g_cycle.front();
	}
	return projectv::present_mode::g_cycle;
}

