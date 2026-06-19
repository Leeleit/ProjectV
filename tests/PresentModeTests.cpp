
#include "render/vulkan/VulkanSwapchain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

#define EXPECT_TRUE(context, expr) ExpectTrue(context, (expr), __LINE__, #expr)
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)

/// \brief **Surface with all three modes (Windows / Linux+X11+VRR).**
///
/// \details
///  The cycle should be `[FIFO, MAILBOX, IMMEDIATE]` in

///  priority order.

void TestPresentModeCycleIncludesAllThree(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(3), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), cycle[1]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), cycle[2]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
}

/// \brief **Surface with only FIFO + MAILBOX (Linux/Wayland without
///
/// \details
///  IMMEDIATE support).** The cycle should be 2 elements

///  `[FIFO, MAILBOX]`. This is the case where the operator

///  reported "4 presses but only 2 unique modes" pre-fix.

void TestPresentModeCycleExcludesUnsupported(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(2), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), cycle[1]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
}

/// \brief **Surface with only FIFO (headless / non-conformant
///
/// \details
///  fallback).** The cycle should be a single element

///  `[FIFO]`, and `CyclePreferredPresentMode` should be a

///  no-op (1-element cycle has no "next").

void TestPresentModeCycleOnlyFifo(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
	/// \brief 1-element cycle:
	///
	/// \details
	/// cycle() is a no-op, mode stays FIFO.
	const VkPresentModeKHR before = GetActivePresentMode();
	CyclePreferredPresentMode();
	EXPECT_EQ(context, before, GetActivePresentMode());
}

/// \brief **Empty surface-present-modes list (defensive — Vulkan
///
/// \details
///  1.4 spec says FIFO is mandatory, so this case is rare).

///  The safety net pushes FIFO into the cycle.**

void TestPresentModeCycleEmptyFallsBackToFifo(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
}

void TestPresentModeCycleRespectsPriorityOrder(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(3), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), cycle[1]);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), cycle[2]);
}

/// \brief **Cycle advances and wraps.** With a 3-mode cycle
///
/// \details
///  `[FIFO, MAILBOX, IMMEDIATE]`, three successive

///  `CyclePreferredPresentMode` calls must visit

///  `MAILBOX → IMMEDIATE → FIFO` (with the initial

///  state set to FIFO by `BuildPresentModeCycle`).

void TestCycleAdvancesAndWrapsThreeMode(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	const VkPresentModeKHR m1 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m1);
	const VkPresentModeKHR m2 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), m2);
	const VkPresentModeKHR m3 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), m3);
	/// \brief Wraps again.
	const VkPresentModeKHR m4 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m4);
}

/// \brief **Cycle advances and wraps — 2-mode cycle
///
/// \details
///  `[FIFO, MAILBOX]`.** This is the case the operator

///  reported as "4 presses but only 2 unique modes" pre-fix.

///  Post-fix: every press advances the cycle.

void TestCycleAdvancesAndWrapsTwoMode(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	const VkPresentModeKHR m1 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m1);
	const VkPresentModeKHR m2 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), m2);
	const VkPresentModeKHR m3 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m3);
	/// \brief 4 presses from FIFO:
	///
	/// \details
	/// FIFO → MAILBOX → FIFO → MAILBOX.
	///  Every press advanced the cycle. The pre-fix

	///  failure mode (4 presses, 2 unique runtime modes)

	///  is gone.

}

/// \brief **Index accessor.** `GetPresentModeCycleIndex(mode)`
///
/// \details
///  returns 0..N-1 for modes in the cycle, 0 for modes

///  outside the cycle. The +1 for HUD display is done at

///  the call site (`GetPresentModeCycleIndex(activeMode) +

///  1u`), not here.

void TestPresentModeCycleIndex(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(0), GetPresentModeCycleIndex(VK_PRESENT_MODE_FIFO_KHR));
	EXPECT_EQ(context, static_cast<std::size_t>(1), GetPresentModeCycleIndex(VK_PRESENT_MODE_MAILBOX_KHR));
	EXPECT_EQ(context, static_cast<std::size_t>(2), GetPresentModeCycleIndex(VK_PRESENT_MODE_IMMEDIATE_KHR));
	/// \brief Mode not in cycle → 0 (FIFO position).
	EXPECT_EQ(context, static_cast<std::size_t>(0), GetPresentModeCycleIndex(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
}

/// \brief **Size accessor.** `GetPresentModeCycleSize()` returns
///
/// \details
///  the number of modes in the cycle, used by the HUD

///  `VSync: <mode> (index/size)` display.

void TestPresentModeCycleSize(TestContext &context)
{
	{
		const std::vector<VkPresentModeKHR> surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		EXPECT_EQ(context, static_cast<std::size_t>(1), GetPresentModeCycleSize());
	}
	{
		const std::vector<VkPresentModeKHR> surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_MAILBOX_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		EXPECT_EQ(context, static_cast<std::size_t>(2), GetPresentModeCycleSize());
	}
	{
		const std::vector<VkPresentModeKHR> surfaceModes{
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
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	/// \brief Initial build:
	///
	/// \details
	/// g_active = FIFO (preserved from reset).
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	/// \brief Press V:
	///
	/// \details
	/// advance to IMMEDIATE.
	const VkPresentModeKHR afterPress = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), afterPress);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), GetActivePresentMode());
	/// \brief **Recreate swapchain** (simulated by calling
	///
	/// \details
	///  `BuildPresentModeCycle` again with the same

	///  surface modes). Pre-fix: `g_active` was reset to

	///  FIFO. Post-fix: `g_active` is preserved as

	///  IMMEDIATE because it's still in the cycle.

	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
}

/// \brief **Cycle rebuild falls back when previous `g_active`
///
/// \details
///  is no longer supported (display hot-swap case).**

void TestPresentModeCycleFallsBackWhenActiveDropped(TestContext &context)
{
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	/// \brief First build:
	///
	/// \details
	/// 2-mode cycle, advance to IMMEDIATE.
	{
		const std::vector<VkPresentModeKHR> surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		CyclePreferredPresentMode();  // advance FIFO -> IMMEDIATE
	}
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), GetActivePresentMode());
	/// \brief Display hot-swap:
	///
	/// \details
	/// new surface only exposes FIFO.
	///  `g_active` (IMMEDIATE) is not in the new cycle,

	///  so we fall back to the highest-priority supported

	///  mode (FIFO).

	{
		const std::vector<VkPresentModeKHR> surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
	}
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		GetActivePresentMode());
}

/// \brief **Cycle walks across multiple swapchain recreates
///
/// \details
///  (the operator's actual scenario).** 4 V presses on

///  a `{FIFO, IMMEDIATE}` cycle with `RecreateSwapchain`

///  after each press must alternate FIFO / IMMEDIATE /

///  FIFO / IMMEDIATE. Pre-fix: stuck on IMMEDIATE every

///  time. Post-fix: alternating.

void TestPresentModeCycleWalksAcrossRecreates(TestContext &context)
{
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	/// \brief Press 1 + recreate:
	///
	/// \details
	/// FIFO -> IMMEDIATE.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
	/// \brief Press 2 + recreate:
	///
	/// \details
	/// IMMEDIATE -> FIFO.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		GetActivePresentMode());
	/// \brief Press 3 + recreate:
	///
	/// \details
	/// FIFO -> IMMEDIATE.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
	/// \brief Press 4 + recreate:
	///
	/// \details
	/// IMMEDIATE -> FIFO.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		GetActivePresentMode());
}
} // namespace

int main()
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
