#pragma once

#include "c_kernels/frustum_cull.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include "core/Types.hpp"

#include <span>
#include <vector>

namespace projectv::c_kernels {


[[nodiscard]] ProjectvCFrustumCullParameters ToCParameters(
	const ChunkCullingParameters &parameters) noexcept;

[[nodiscard]] std::vector<ModelInstanceData> FilterVisibleInstances(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);


[[nodiscard]] std::vector<uint8_t> CullVisibleMask(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);


constexpr std::size_t kBatchDispatchThreshold = 8;

} // namespace projectv::c_kernels

