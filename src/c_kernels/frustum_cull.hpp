#pragma once

#include <cstddef>
#include <cstdint>

namespace projectv::c_kernels {

struct ProjectvCFrustumCullParameters {
	float cameraPosition[3];
	float maxDistance;
	float cameraForward[3];
	float tanHalfVerticalFov;
	float cameraRight[3];
	float tanHalfHorizontalFov;
	float cameraUp[3];
	float nearPlane;
};

struct ProjectvCAabb {
	float min[3];
	float _pad0;
	float max[3];
	float _pad1;
};

void projectv_cull_frustum_scalar(
	std::uint8_t *visible_mask,
	const ProjectvCAabb *aabbs,
	const ProjectvCFrustumCullParameters &parameters,
	std::size_t count) noexcept;

#if defined(__AVX2__)
void projectv_cull_frustum_avx2(
	std::uint8_t *visible_mask,
	const ProjectvCAabb *aabbs,
	const ProjectvCFrustumCullParameters &parameters,
	std::size_t count) noexcept;
#endif

} // namespace projectv::c_kernels