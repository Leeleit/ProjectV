#include "core/RuntimeDiagnostics.hpp"

#include "SDL3/SDL.h"
#include "render/vulkan/VulkanResult.hpp"

#include "fmt/format.h"

#include <cstdlib>

namespace {
void LogFormattedMessage(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view detail)
{
	const std::string message = fmt::format("[ProjectV][{}][{}] {}", subsystem, step, detail);
	SDL_Log("%s", message.c_str());
}
} // namespace

namespace runtime {
void LogRuntimeFailure(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view detail)
{
	LogFormattedMessage(subsystem, step, detail);
}

void LogVkFailure(const std::string_view step, const VkResult result)
{
	LogRuntimeFailure(
		"Vulkan",
		step,
		fmt::format("{} ({})", VkResultToString(result), static_cast<int>(result)));
}

void LogVmaFailure(const std::string_view step, const VkResult result)
{
	LogRuntimeFailure(
		"VMA",
		step,
		fmt::format("{} ({})", VkResultToString(result), static_cast<int>(result)));
}

void LogSdlFailure(const std::string_view step)
{
	const char *error = SDL_GetError();
	LogRuntimeFailure(
		"SDL",
		step,
		error && *error ? error : "SDL_GetError returned an empty message");
}

void LogCheckFailure(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view condition,
	const std::string_view detail,
	const char *file,
	const int line)
{
	LogRuntimeFailure(
		subsystem,
		step,
		fmt::format(
			"check '{}' failed at {}:{}: {}",
			condition,
			file ? file : "<unknown>",
			line,
			detail));
}

[[noreturn]] void AbortAssertFailure(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view condition,
	const std::string_view detail,
	const char *file,
	const int line)
{
	LogRuntimeFailure(
		subsystem,
		step,
		fmt::format(
			"assert '{}' failed at {}:{}: {}",
			condition,
			file ? file : "<unknown>",
			line,
			detail));
	SDL_TriggerBreakpoint();
	std::abort();
}

} // namespace runtime
