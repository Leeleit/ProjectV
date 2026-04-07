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
bool LogRuntimeFailure(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view detail)
{
	LogFormattedMessage(subsystem, step, detail);
	return false;
}

bool LogVkFailure(const std::string_view step, const VkResult result)
{
	return LogRuntimeFailure(
		"Vulkan",
		step,
		fmt::format("{} ({})", VkResultToString(result), static_cast<int>(result)));
}

bool LogVmaFailure(const std::string_view step, const VkResult result)
{
	return LogRuntimeFailure(
		"VMA",
		step,
		fmt::format("{} ({})", VkResultToString(result), static_cast<int>(result)));
}

bool LogSdlFailure(const std::string_view step)
{
	const char *error = SDL_GetError();
	return LogRuntimeFailure(
		"SDL",
		step,
		(error && *error) ? error : "SDL_GetError returned an empty message");
}

bool LogCheckFailure(
	const std::string_view subsystem,
	const std::string_view step,
	const std::string_view condition,
	const std::string_view detail,
	const char *file,
	const int line)
{
	return LogRuntimeFailure(
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
