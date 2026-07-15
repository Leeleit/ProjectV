
#include "render/vulkan/VulkanSwapchain.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

namespace {
struct TestContext {
	int failures = 0;

	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

template <typename T>
void ExpectEqual(
	TestContext &context,
	const T &expected,
	const T &actual,
	const int line,
	const std::string_view expr)
{
	if (!(expected == actual)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.*s (expected %lld, got %lld)",
			static_cast<int>(expr.size()),
			expr.data(),
			static_cast<long long>(expected),
			static_cast<long long>(actual));
		context.Fail(line, buffer);
	}
}

[[maybe_unused]] void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

#define EXPECT_TRUE(context, expr) ExpectTrue(context, (expr), __LINE__, #expr)
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)

void TestPresentModeCycleIncludesAllThree(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(3), cycle.size());
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, cycle[0]);
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, cycle[1]);
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, cycle[2]);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
}

void TestPresentModeCycleExcludesUnsupported(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(2), cycle.size());
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, cycle[0]);
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, cycle[1]);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
}

void TestPresentModeCycleOnlyFifo(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, cycle[0]);
	const VkPresentModeKHR before = GetActivePresentMode();
	CyclePreferredPresentMode();
	EXPECT_EQ(context, before, GetActivePresentMode());
}

void TestPresentModeCycleEmptyFallsBackToFifo(TestContext &context)
{
#if !defined(_WIN32)
	constexpr std::vector<VkPresentModeKHR> surfaceModes{}; // libc++: constexpr vector; MSVC STL: not yet
#else
	const std::vector<VkPresentModeKHR> surfaceModes{};
#endif
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, cycle[0]);
}

void TestPresentModeCycleRespectsPriorityOrder(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(3), cycle.size());
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, cycle[0]);
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, cycle[1]);
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, cycle[2]);
}

void TestCycleAdvancesAndWrapsThreeMode(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
	const VkPresentModeKHR m1 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, m1);
	const VkPresentModeKHR m2 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, m2);
	const VkPresentModeKHR m3 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, m3);
	const VkPresentModeKHR m4 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, m4);
}

void TestCycleAdvancesAndWrapsTwoMode(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
	const VkPresentModeKHR m1 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, m1);
	const VkPresentModeKHR m2 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, m2);
	const VkPresentModeKHR m3 = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_MAILBOX_KHR, m3);
}

void TestPresentModeCycleIndex(TestContext &context)
{
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(0), GetPresentModeCycleIndex(VK_PRESENT_MODE_FIFO_KHR));
	EXPECT_EQ(context, static_cast<std::size_t>(1), GetPresentModeCycleIndex(VK_PRESENT_MODE_MAILBOX_KHR));
	EXPECT_EQ(context, static_cast<std::size_t>(2), GetPresentModeCycleIndex(VK_PRESENT_MODE_IMMEDIATE_KHR));
	EXPECT_EQ(context, static_cast<std::size_t>(0), GetPresentModeCycleIndex(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
}

void TestPresentModeCycleSize(TestContext &context)
{
	{
		const std::vector surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		EXPECT_EQ(context, static_cast<std::size_t>(1), GetPresentModeCycleSize());
	}
	{
		const std::vector surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_MAILBOX_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		EXPECT_EQ(context, static_cast<std::size_t>(2), GetPresentModeCycleSize());
	}
	{
		const std::vector surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_MAILBOX_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		EXPECT_EQ(context, static_cast<std::size_t>(3), GetPresentModeCycleSize());
	}
}

void TestPresentModeCyclePreservesActiveAcrossRebuild(TestContext &context)
{
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
	const VkPresentModeKHR afterPress = CyclePreferredPresentMode();
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, afterPress);
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, GetActivePresentMode());

	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		GetActivePresentMode());
}

void TestPresentModeCycleFallsBackWhenActiveDropped(TestContext &context)
{
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	{
		const std::vector surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		CyclePreferredPresentMode(); // advance FIFO -> IMMEDIATE
	}
	EXPECT_EQ(context, VK_PRESENT_MODE_IMMEDIATE_KHR, GetActivePresentMode());

	{
		const std::vector surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
	}
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_FIFO_KHR,
		GetActivePresentMode());
}

void TestPresentModeCycleWalksAcrossRecreates(TestContext &context)
{
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	const std::vector surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, VK_PRESENT_MODE_FIFO_KHR, GetActivePresentMode());
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		GetActivePresentMode());
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_FIFO_KHR,
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_FIFO_KHR,
		GetActivePresentMode());
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		GetActivePresentMode());
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_FIFO_KHR,
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		VK_PRESENT_MODE_FIFO_KHR,
		GetActivePresentMode());
}
} // namespace

int main() // NOLINT(*-exception-escape)
{
	TestContext context{};

	TestPresentModeCycleIncludesAllThree(context);
	TestPresentModeCycleExcludesUnsupported(context);
	TestPresentModeCycleOnlyFifo(context);
	TestPresentModeCycleEmptyFallsBackToFifo(context);
	TestPresentModeCycleRespectsPriorityOrder(context);
	TestCycleAdvancesAndWrapsThreeMode(context);
	TestCycleAdvancesAndWrapsTwoMode(context);
	TestPresentModeCycleIndex(context);
	TestPresentModeCycleSize(context);
	TestPresentModeCyclePreservesActiveAcrossRebuild(context);
	TestPresentModeCycleFallsBackWhenActiveDropped(context);
	TestPresentModeCycleWalksAcrossRecreates(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVPresentModeTests passed");
	return EXIT_SUCCESS;
}
