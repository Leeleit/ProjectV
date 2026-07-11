import projectv.math;

#include "app/Camera.hpp"
#include "core/Types.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

// noinspection DfaUnreachableFunctionCall
void Expect(const bool condition, const char *label)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", label);
		std::abort();
	}
}

void ExpectNear(const float a, const float b, const float epsilon, const char *label)
{
	Expect(std::abs(a - b) <= epsilon, label);
}

CameraState MakeIdentityCamera()
{
	CameraState camera{};
	camera.position = projectv::math::Vec3{0.0f, 0.0f, 0.0f, 0.0f};
	camera.yawRadians = 0.0f;
	camera.pitchRadians = 0.0f;
	camera.moveSpeed = 10.0f;
	camera.mouseSensitivity = 0.0025f;
	camera.verticalFovRadians = 1.0471976f;
	camera.nearPlane = 0.1f;
	camera.farPlane = 128.0f;
	camera.controlMode = CameraState::ControlMode::Creative;
	return camera;
}

constexpr float kEpsilon = 1e-5f;

} // namespace

int main()
{

	{
		const CameraState camera = MakeIdentityCamera();
		constexpr VkExtent2D extent{1280u, 720u};
		const GraphicsPushConstants pc =
			BuildGraphicsPushConstants(camera, extent);
		ExpectNear(
			pc.cameraPosition[0], 0.0f, kEpsilon,
			"identity: cameraPosition.x == 0");
		ExpectNear(
			pc.cameraPosition[1], 0.0f, kEpsilon,
			"identity: cameraPosition.y == 0");
		ExpectNear(
			pc.cameraPosition[2], 0.0f, kEpsilon,
			"identity: cameraPosition.z == 0");
		ExpectNear(
			pc.cameraPosition[3], 0.1f, kEpsilon,
			"identity: cameraPosition.w == nearPlane");
		ExpectNear(
			pc.cameraForward[3], 128.0f, kEpsilon,
			"identity: cameraForward.w == farPlane");
	}
	std::printf("[OK] identity: pass-through of position/near/far\n");


	{
		const CameraState camera = MakeIdentityCamera();
		constexpr VkExtent2D extent{1280u, 720u};
		const GraphicsPushConstants pc =
			BuildGraphicsPushConstants(camera, extent);
		const projectv::math::Vec4 &m0 = pc.viewProjection.c[0];
		const projectv::math::Vec4 &m1 = pc.viewProjection.c[1];
		const projectv::math::Vec4 &m2 = pc.viewProjection.c[2];
		const projectv::math::Vec4 &m3 = pc.viewProjection.c[3];
		const float aspect =
			static_cast<float>(extent.width) / static_cast<float>(extent.height);
		const float tanHalfFov = std::tan(camera.verticalFovRadians * 0.5f);
		ExpectNear(
			m0[0], 1.0f / (aspect * tanHalfFov), kEpsilon,
			"identity: VP m00 = 1/(aspect*tanHalfFov)");
		ExpectNear(m1[1], -1.0f / tanHalfFov, kEpsilon, "identity: VP m11 = -1/tanHalfFov");
		ExpectNear(m2[2], camera.farPlane / (camera.nearPlane - camera.farPlane), kEpsilon, "identity: VP m22 projection z");
		ExpectNear(m2[3], -1.0f, kEpsilon, "identity: VP m23 = -1");
		ExpectNear(m3[3], 0.0f, kEpsilon, "identity: VP m33 = 0");
	}
	std::printf("[OK] identity: projection matrix coefficients\n");


	{
		CameraState camera = MakeIdentityCamera();
		camera.position = projectv::math::Vec3{5.0f, -3.0f, 12.0f, 0.0f};
		camera.yawRadians = 0.0f;
		camera.pitchRadians = -0.3f;
		constexpr VkExtent2D extent{1920u, 1080u};
		const GraphicsPushConstants pc =
			BuildGraphicsPushConstants(camera, extent);
		ExpectNear(pc.cameraPosition[0], 5.0f, kEpsilon, "translated: pos.x");
		ExpectNear(pc.cameraPosition[1], -3.0f, kEpsilon, "translated: pos.y");
		ExpectNear(pc.cameraPosition[2], 12.0f, kEpsilon, "translated: pos.z");
		const float lenSq = pc.cameraForward[0] * pc.cameraForward[0] +
			pc.cameraForward[1] * pc.cameraForward[1] +
			pc.cameraForward[2] * pc.cameraForward[2];
		ExpectNear(lenSq, 1.0f, kEpsilon, "translated: cameraForward is unit length");
	}
	std::printf("[OK] translated: position+forward correct, forward unit-length\n");


	{
		const CameraState camera = MakeIdentityCamera();
		constexpr VkExtent2D extent{1u, 1u};
		const GraphicsPushConstants pc =
			BuildGraphicsPushConstants(camera, extent);
		Expect(std::isfinite(pc.viewProjection.c[0][0]),
			"tiny-extent: VP m00 is finite");
	}
	std::printf("[OK] tiny extent: no inf propagation\n");


	{
		const CameraState camera = MakeIdentityCamera();
		constexpr VkExtent2D extent{1920u, 0u};
		const GraphicsPushConstants pc =
			BuildGraphicsPushConstants(camera, extent);
		Expect(std::isfinite(pc.viewProjection.c[0][0]),
			"zero-height: VP m00 is finite (no inf)");
		ExpectNear(
			pc.viewProjection.c[0][0],
			1.0f / (1.0f * std::tan(camera.verticalFovRadians * 0.5f)),
			kEpsilon,
			"zero-height: aspect falls back to 1.0 (square ratio)");
	}
	std::printf("[OK] zero-height: aspect guard prevents inf propagation\n");

	std::printf("ProjectVGraphicsPushConstantsTests: 5/5 passed\n");
	return EXIT_SUCCESS;
}