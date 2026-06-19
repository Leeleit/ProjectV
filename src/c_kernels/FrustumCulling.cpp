#include "c_kernels/FrustumCulling.hpp"

#include "render/SceneResources.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace projectv::c_kernels {

namespace {

/// \brief **AoS → C-kernel AoS conversion.** 300 `ModelInstanceData`
///
/// \details
///  AABBs = 300 `ProjectvCAabb` = 300 * 32 B = 9.6 KB.

///  The conversion is 6 stores per AABB (min[0..2] +

///  max[0..2] + 2 pad stores) — scalar stores, the

///  compiler turns them into 3 `vmovss` (or 1 `vmovups`

///  for the full 32 B with alignment hints).

[[nodiscard]] std::vector<ProjectvCAabb> ToCAabbs(
	const std::span<const ModelInstanceData> &instances) {
	std::vector<ProjectvCAabb> aabbs(instances.size());
	for (std::size_t i = 0; i < instances.size(); ++i) {
		const auto &src = instances[i];
		auto &dst = aabbs[i];
		dst.min[0] = src.worldAabbMin.x;
		dst.min[1] = src.worldAabbMin.y;
		dst.min[2] = src.worldAabbMin.z;
		dst._pad0 = 0.0f;
		dst.max[0] = src.worldAabbMax.x;
		dst.max[1] = src.worldAabbMax.y;
		dst.max[2] = src.worldAabbMax.z;
		dst._pad1 = 0.0f;
	}
	return aabbs;
}

} // namespace

ProjectvCFrustumCullParameters ToCParameters(
	const ChunkCullingParameters &parameters) noexcept {
	ProjectvCFrustumCullParameters out{};
	out.cameraPosition[0] = parameters.cameraPositionAndMaxDistance.x;
	out.cameraPosition[1] = parameters.cameraPositionAndMaxDistance.y;
	out.cameraPosition[2] = parameters.cameraPositionAndMaxDistance.z;
	out.maxDistance = parameters.cameraPositionAndMaxDistance.w;
	out.cameraForward[0] = parameters.cameraForwardAndTanHalfVerticalFov.x;
	out.cameraForward[1] = parameters.cameraForwardAndTanHalfVerticalFov.y;
	out.cameraForward[2] = parameters.cameraForwardAndTanHalfVerticalFov.z;
	out.tanHalfVerticalFov = std::max(
		parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	out.cameraRight[0] = parameters.cameraRightAndTanHalfHorizontalFov.x;
	out.cameraRight[1] = parameters.cameraRightAndTanHalfHorizontalFov.y;
	out.cameraRight[2] = parameters.cameraRightAndTanHalfHorizontalFov.z;
	out.tanHalfHorizontalFov = std::max(
		parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	out.cameraUp[0] = parameters.cameraUpAndNearPlane.x;
	out.cameraUp[1] = parameters.cameraUpAndNearPlane.y;
	out.cameraUp[2] = parameters.cameraUpAndNearPlane.z;
	out.nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);
	return out;
}

std::vector<uint8_t> CullVisibleMask(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters) {
	std::vector<ProjectvCAabb> aabbs = ToCAabbs(instances);
	const ProjectvCFrustumCullParameters cparams = ToCParameters(parameters);
	std::vector<uint8_t> mask((instances.size() + 7) / 8, 0);
	if (instances.empty()) {
		return mask;
	}
	projectv_cull_frustum_scalar(
		mask.data(),
		aabbs.data(),
		&cparams,
		instances.size());
	return mask;
}

std::vector<ModelInstanceData> FilterVisibleInstances(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters) {
	std::vector<ModelInstanceData> visible;
	if (instances.size() < kBatchDispatchThreshold) {
		/// \brief **Fallback to inline C++ helper** for tiny
		///
		/// \details
		///  inputs. Avoids the kernel's per-batch setup

		///  cost (~3 µs of fixed overhead) when we have

		///  fewer AABBs than a single SIMD batch.

		visible.reserve(instances.size());
		for (const auto &instance : instances) {
			if (IsAabbVisibleAgainstCameraFrustum(
					instance.worldAabbMin,
					instance.worldAabbMax,
					parameters)) {
				visible.push_back(instance);
			}
		}
		return visible;
	}

	const std::vector<uint8_t> mask = CullVisibleMask(instances, parameters);
	visible.reserve(instances.size());
	for (std::size_t i = 0; i < instances.size(); ++i) {
		const std::uint8_t bit = static_cast<std::uint8_t>(1u << (i % 8));
		if ((mask[i / 8] & bit) != 0) {
			visible.push_back(instances[i]);
		}
	}
	return visible;
}

} // namespace projectv::c_kernels
