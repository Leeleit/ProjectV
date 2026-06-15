// **Present-mode cycle tests (auto-detect cycle,
// `2026-06-14`).**
//
// Verifies the runtime present-mode cycle logic in
// `src/render/vulkan/VulkanSwapchain.hpp/cpp`:
//   * `BuildPresentModeCycle(surfacePresentModes)` constructs
//     a cycle vector containing only the surface's exposed
//     modes, in priority order `{FIFO, MAILBOX, IMMEDIATE}`.
//   * `CyclePreferredPresentMode()` advances the cycle by
//     one step, wrapping at the end.
//   * `GetActivePresentMode()` / `GetPresentModeCycleSize()`
//     / `GetPresentModeCycleIndex(mode)` reflect the cycle
//     state read-only.
//
// The tests reset the cycle state at the start of each test
// by calling `BuildPresentModeCycle` with a controlled
// surface-present-modes vector. The inline global
// `projectv::present_mode::g_active` is set by
// `BuildPresentModeCycle` to the first element of the
// returned cycle, so we don't need to manually reset it.
//
// The previous 3-mode hard-coded cycle (`FIFO → IMMEDIATE →
// MAILBOX → FIFO`) had a failure mode on Linux/Wayland
// surfaces that don't expose IMMEDIATE: the operator
// reported "4 presses but only 2 unique runtime modes" —
// the IMMEDIATE step silently fell through to MAILBOX (via
// `PickBestAvailablePresentMode`). The new auto-detect
// cycle builds the cycle from the actual surface support,
// so this failure mode is gone. The user-visible cycle
// length matches the number of physically-supported modes.

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

// **Surface with all three modes (Windows / Linux+X11+VRR).**
// The cycle should be `[FIFO, MAILBOX, IMMEDIATE]` in
// priority order.
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

// **Surface with only FIFO + MAILBOX (Linux/Wayland without
// IMMEDIATE support).** The cycle should be 2 elements
// `[FIFO, MAILBOX]`. This is the case where the operator
// reported "4 presses but only 2 unique modes" pre-fix.
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

// **Surface with only FIFO (headless / non-conformant
// fallback).** The cycle should be a single element
// `[FIFO]`, and `CyclePreferredPresentMode` should be a
// no-op (1-element cycle has no "next").
void TestPresentModeCycleOnlyFifo(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
	};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
	// 1-element cycle: cycle() is a no-op, mode stays FIFO.
	const VkPresentModeKHR before = GetActivePresentMode();
	CyclePreferredPresentMode();
	EXPECT_EQ(context, before, GetActivePresentMode());
}

// **Empty surface-present-modes list (defensive — Vulkan
// 1.4 spec says FIFO is mandatory, so this case is rare).
// The safety net pushes FIFO into the cycle.**
void TestPresentModeCycleEmptyFallsBackToFifo(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{};
	const std::vector<VkPresentModeKHR> cycle = BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<std::size_t>(1), cycle.size());
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), cycle[0]);
}

// **Modes appear in the surface support in a different
// order than the priority list — the cycle must be in
// priority order, not surface order.** Some Wayland
// compositors expose modes in {IMMEDIATE, FIFO, MAILBOX}
// order; the cycle must always be `{FIFO, MAILBOX,
// IMMEDIATE}` so the operator's "first press lands on the
// no-cap mode" comment from the 2026-06-13 cycle becomes
// the correct new behavior (first press lands on MAILBOX
// if supported — the "tear-free" mode is the new default
// for benchmarking, vs the old IMMEDIATE step).
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

// **Cycle advances and wraps.** With a 3-mode cycle
// `[FIFO, MAILBOX, IMMEDIATE]`, three successive
// `CyclePreferredPresentMode` calls must visit
// `MAILBOX → IMMEDIATE → FIFO` (with the initial
// state set to FIFO by `BuildPresentModeCycle`).
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
	// Wraps again.
	const VkPresentModeKHR m4 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m4);
}

// **Cycle advances and wraps — 2-mode cycle
// `[FIFO, MAILBOX]`.** This is the case the operator
// reported as "4 presses but only 2 unique modes" pre-fix.
// Post-fix: every press advances the cycle.
void TestCycleAdvancesAndWrapsTwoMode(TestContext &context)
{
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
	};
	// **2026-06-14 fix:** explicitly reset `g_active` to
	// FIFO before this test, since the previous test
	// (3-mode walk) leaves `g_active = MAILBOX`. The
	// post-2026-06-14-evening `BuildPresentModeCycle`
	// preserves `g_active` if it's in the new cycle, so
	// simply calling `BuildPresentModeCycle({FIFO,
	// MAILBOX})` here would carry MAILBOX over from the
	// previous test. Build with `{FIFO}` first to force
	// the fallback to FIFO, then build the actual 2-mode
	// cycle. This pattern is the documented "test reset
	// for cycle-preservation semantics".
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	const VkPresentModeKHR m1 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m1);
	const VkPresentModeKHR m2 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), m2);
	const VkPresentModeKHR m3 = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR), m3);
	// 4 presses from FIFO: FIFO → MAILBOX → FIFO → MAILBOX.
	// Every press advanced the cycle. The pre-fix
	// failure mode (4 presses, 2 unique runtime modes)
	// is gone.
}

// **Index accessor.** `GetPresentModeCycleIndex(mode)`
// returns 0..N-1 for modes in the cycle, 0 for modes
// outside the cycle. The +1 for HUD display is done at
// the call site (`GetPresentModeCycleIndex(activeMode) +
// 1u`), not here.
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
	// Mode not in cycle → 0 (FIFO position).
	EXPECT_EQ(context, static_cast<std::size_t>(0), GetPresentModeCycleIndex(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
}

// **Size accessor.** `GetPresentModeCycleSize()` returns
// the number of modes in the cycle, used by the HUD
// `VSync: <mode> (index/size)` display.
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

// **Cycle rebuild preserves `g_active` (2026-06-14 fix).**
// The operator reported "V hotkey stuck on IMMEDIATE":
// every V press logged `IMMEDIATE [cycle 2/2]`. Root
// cause: `BuildPresentModeCycle` unconditionally reset
// `g_active = g_cycle.front()` (FIFO) on every rebuild,
// and the V hotkey triggers a `RecreateSwapchain` after
// each press — so the cycle advance was followed by an
// immediate reset to FIFO, and the next press would
// advance from FIFO back to IMMEDIATE. Net effect: the
// cycle appeared stuck. **Fix:** capture the previous
// `g_active` before rebuild; if it's still in the new
// cycle, keep it. Display hot-swap (mode dropped) falls
// back to highest-priority supported.
void TestPresentModeCyclePreservesActiveAcrossRebuild(TestContext &context)
{
	// **2026-06-14 fix:** explicit reset to FIFO before
	// this test (see comment in
	// `TestCycleAdvancesAndWrapsTwoMode` for the
	// rationale). Previous tests may leave `g_active`
	// in some non-FIFO state.
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	// Initial build: g_active = FIFO (preserved from reset).
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	// Press V: advance to IMMEDIATE.
	const VkPresentModeKHR afterPress = CyclePreferredPresentMode();
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), afterPress);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), GetActivePresentMode());
	// **Recreate swapchain** (simulated by calling
	// `BuildPresentModeCycle` again with the same
	// surface modes). Pre-fix: `g_active` was reset to
	// FIFO. Post-fix: `g_active` is preserved as
	// IMMEDIATE because it's still in the cycle.
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
}

// **Cycle rebuild falls back when previous `g_active`
// is no longer supported (display hot-swap case).**
void TestPresentModeCycleFallsBackWhenActiveDropped(TestContext &context)
{
	// **2026-06-14 fix:** explicit reset to FIFO before
	// this test (see comment in
	// `TestCycleAdvancesAndWrapsTwoMode` for the
	// rationale). Previous tests leave `g_active` in
	// some non-FIFO state.
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	// First build: 2-mode cycle, advance to IMMEDIATE.
	{
		const std::vector<VkPresentModeKHR> surfaceModes{
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
		};
		(void)BuildPresentModeCycle(surfaceModes);
		CyclePreferredPresentMode();  // advance FIFO -> IMMEDIATE
	}
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR), GetActivePresentMode());
	// Display hot-swap: new surface only exposes FIFO.
	// `g_active` (IMMEDIATE) is not in the new cycle,
	// so we fall back to the highest-priority supported
	// mode (FIFO).
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

// **Cycle walks across multiple swapchain recreates
// (the operator's actual scenario).** 4 V presses on
// a `{FIFO, IMMEDIATE}` cycle with `RecreateSwapchain`
// after each press must alternate FIFO / IMMEDIATE /
// FIFO / IMMEDIATE. Pre-fix: stuck on IMMEDIATE every
// time. Post-fix: alternating.
void TestPresentModeCycleWalksAcrossRecreates(TestContext &context)
{
	// **2026-06-14 fix:** explicit reset to FIFO before
	// this test (see comment in
	// `TestCycleAdvancesAndWrapsTwoMode` for the
	// rationale).
	(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});
	const std::vector<VkPresentModeKHR> surfaceModes{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(context, static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR), GetActivePresentMode());
	// Press 1 + recreate: FIFO -> IMMEDIATE.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
	// Press 2 + recreate: IMMEDIATE -> FIFO.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR),
		GetActivePresentMode());
	// Press 3 + recreate: FIFO -> IMMEDIATE.
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		CyclePreferredPresentMode());
	(void)BuildPresentModeCycle(surfaceModes);
	EXPECT_EQ(
		context,
		static_cast<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR),
		GetActivePresentMode());
	// Press 4 + recreate: IMMEDIATE -> FIFO.
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
