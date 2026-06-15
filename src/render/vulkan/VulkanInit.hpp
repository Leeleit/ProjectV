#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include "render/vulkan/VulkanSwapchain.hpp"

namespace projectv::vulkan_init {

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `InitVulkan`. Cold path (1× per session startup), so the
// `std::expected` cost is irrelevant. The error variants are
// grouped by init stage (Precondition, Bootstrap, Tracy,
// Swapchain, World, Ecs, Physics, SceneResources, Pipelines,
// Manifest). The per-step `runtime::LogRuntimeFailure` log
// line is preserved inside the implementation — the variant
// is the machine-readable signal for the caller.
enum class VulkanInitError : std::uint8_t {
	PreconditionFailed = 0,
	BootstrapFailed,
	BootstrapFailureProbe,
	TracyContextFailed,
	SwapchainFailed,
	WorldCreationFailed,
	WorldFailureProbe,
	EcsSyncFailed,
	PhysicsStateFailed,
	SceneResourcesFailed,
	SceneResourcesFailureProbe,
	GraphicsPipelineProbe,
	GraphicsPipelineFailed,
	VoxelMeshingPipelineProbe,
	VoxelMeshingPipelineFailed,
	ModelPipelineFailed,
	ModelManifestFailed,
};

constexpr std::string_view toString(VulkanInitError e) noexcept {
	switch (e) {
	case VulkanInitError::PreconditionFailed: return "PreconditionFailed";
	case VulkanInitError::BootstrapFailed: return "BootstrapFailed";
	case VulkanInitError::BootstrapFailureProbe: return "BootstrapFailureProbe";
	case VulkanInitError::TracyContextFailed: return "TracyContextFailed";
	case VulkanInitError::SwapchainFailed: return "SwapchainFailed";
	case VulkanInitError::WorldCreationFailed: return "WorldCreationFailed";
	case VulkanInitError::WorldFailureProbe: return "WorldFailureProbe";
	case VulkanInitError::EcsSyncFailed: return "EcsSyncFailed";
	case VulkanInitError::PhysicsStateFailed: return "PhysicsStateFailed";
	case VulkanInitError::SceneResourcesFailed: return "SceneResourcesFailed";
	case VulkanInitError::SceneResourcesFailureProbe: return "SceneResourcesFailureProbe";
	case VulkanInitError::GraphicsPipelineProbe: return "GraphicsPipelineProbe";
	case VulkanInitError::GraphicsPipelineFailed: return "GraphicsPipelineFailed";
	case VulkanInitError::VoxelMeshingPipelineProbe: return "VoxelMeshingPipelineProbe";
	case VulkanInitError::VoxelMeshingPipelineFailed: return "VoxelMeshingPipelineFailed";
	case VulkanInitError::ModelPipelineFailed: return "ModelPipelineFailed";
	case VulkanInitError::ModelManifestFailed: return "ModelManifestFailed";
	}
	return "Unknown";
}
} // namespace projectv::vulkan_init

// **Tier 1.B (`2026-06-13`).** Returns
// `std::expected<void, projectv::vulkan_init::VulkanInitError>`.
// The function still mutates `AppState` in place (the init
// path is the canonical "build the whole thing" entry point);
// the error variant is the only new return value. The TODO's
// `std::expected<VulkanContext, VulkanInitError>` design would
// require a separate "build context" struct + move semantics
// on `AppState` — out of scope for Tier 1 (and not necessary
// for the cold-path type-safety win).
std::expected<void, projectv::vulkan_init::VulkanInitError> InitVulkan(AppState *state);

