#include "render/vulkan/VulkanBootstrap.hpp"
#include "projectv_test_utils.hpp"

int main()
{
	TestContext context;
	EXPECT_EQ(context, VK_API_VERSION_1_4, projectv::render::GetMinVulkanApiVersion());
	constexpr VulkanContextState state{};
	EXPECT_EQ(context, static_cast<VkPipelineCache>(VK_NULL_HANDLE), state.pipelineCache); // VK_NULL_HANDLE is nullptr_t on MSVC Vulkan headers; cast keeps EXPECT_EQ monomorphic
	return context.failures;
}
