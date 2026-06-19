#pragma once

#include "c_kernels/frustum_cull.hpp"
#include "core/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace projectv::c_kernels {

/// \brief **Per-call conversion helper.** Translates the
///
/// \details
///  engine's `ChunkCullingParameters` (Vec4 / Vec3

///  fields) into the C kernel's `ProjectvCFrustumCullParameters`

///  POD. The `w` components are unpacked into the

///  scalar fields (maxDistance, tanHalfVerticalFov, etc.)

///  the kernel reads. The `tanHalfVerticalFov` etc. are

///  clamped to >= 0 here to mirror the

///  `std::max(parameters.*.w, 0.0f)` clamps in the

///  C++ helper.

[[nodiscard]] ProjectvCFrustumCullParameters ToCParameters(
	const ChunkCullingParameters &parameters) noexcept;

[[nodiscard]] std::vector<ModelInstanceData> FilterVisibleInstances(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);

/// \brief **Lower-level entry point.** Returns the raw
///
/// \details
///  8-bits-per-byte visible mask (bit i of byte i/8

///  is set iff AABB i is visible). The output vector

///  is `(count + 7) / 8` bytes long, allocated

///  freshly per call. Used by `FilterVisibleInstances`

///  and by the unit test

///  `ProjectVCFrustumCullingTest` (forthcoming).

[[nodiscard]] std::vector<uint8_t> CullVisibleMask(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);

/// \brief **Crossover threshold.** Below this many AABBs,
///
/// \details
///  the C kernel's per-batch setup cost is not

///  amortised. Engine code should fall back to the

///  inline `IsAabbVisibleAgainstCameraFrustum`

///  helper for `instances.size() < kBatchDispatchThreshold`.

///  The 8-AABB threshold matches the kernel's lane

///  width (the first full AVX2 batch covers exactly

///  8 AABBs).

constexpr std::size_t kBatchDispatchThreshold = 8;

} // namespace projectv::c_kernels

