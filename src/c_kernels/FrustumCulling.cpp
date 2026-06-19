#include "c_kernels/FrustumCulling.hpp"

#include "render/SceneResources.hpp"

#include <algorithm>
#include <vector>

namespace projectv::c_kernels {

namespace {

[[nodiscard]] std::vector<ProjectvCAabb> ToCAabbs(
	const std::span<const ModelInstanceData> &instances)
{
	std::vector<ProjectvCAabb> aabbs(instances.size());
	for (std::size_t i = 0; i < instances.size(); ++i) {
		const auto &src = instances[i];
		auto &[min, _pad0, max, _pad1] = aabbs[i];
		min[0] = src.worldAabbMin.x;
		min[1] = src.worldAabbMin.y;
		min[2] = src.worldAabbMin.z;
		_pad0 = 0.0f;
		max[0] = src.worldAabbMax.x;
		max[1] = src.worldAabbMax.y;
		max[2] = src.worldAabbMax.z;
		_pad1 = 0.0f;
	}
	return aabbs;
}

} // namespace

ProjectvCFrustumCullParameters ToCParameters(
	const ChunkCullingParameters &parameters) noexcept
{
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
	const ChunkCullingParameters &parameters)
{
	const std::vector<ProjectvCAabb> aabbs = ToCAabbs(instances);
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
	const ChunkCullingParameters &parameters)
{
	std::vector<ModelInstanceData> visible;
	if (instances.size() < kBatchDispatchThreshold) {

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
