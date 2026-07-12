#include "core/Types.hpp"

#include <type_traits>

namespace {

static_assert(std::is_same_v<decltype(std::declval<AppState &>().platform()), PlatformState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().context()), VulkanContextState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().swapchain()), SwapchainState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().world()), WorldState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().render()), RenderState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().frame()), FrameState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().simulation()), SimulationState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().input()), InputState &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().ecs()), EcsStatePtr &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().physics()), PhysicsStatePtr &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().audio()), AudioEnginePtr &>);
static_assert(std::is_same_v<decltype(std::declval<AppState &>().shutdownDone()), bool &>);

} // namespace

int main()
{
	return 0;
}
