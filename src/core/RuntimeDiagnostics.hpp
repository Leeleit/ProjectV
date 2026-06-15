#pragma once

#include "volk.h"

#include <string_view>

namespace runtime {
void LogRuntimeFailure(std::string_view subsystem, std::string_view step, std::string_view detail);
void LogVkFailure(std::string_view step, VkResult result);
void LogVmaFailure(std::string_view step, VkResult result);
void LogSdlFailure(std::string_view step);
void LogCheckFailure(
	std::string_view subsystem,
	std::string_view step,
	std::string_view condition,
	std::string_view detail,
	const char *file,
	int line);
[[noreturn]] void AbortAssertFailure(
	std::string_view subsystem,
	std::string_view step,
	std::string_view condition,
	std::string_view detail,
	const char *file,
	int line);
} // namespace runtime

#define PV_CHECK_OR_RETURN(condition, subsystem, step, detail)                                   \
	do {                                                                                         \
		if (!(condition)) {                                                                      \
			::runtime::LogCheckFailure(subsystem, step, #condition, detail, __FILE__, __LINE__); \
			return false;                                                                        \
		}                                                                                        \
	} while (0)

#if !defined(NDEBUG)
#define PV_ASSERT(condition, subsystem, step, detail)                                               \
	do {                                                                                            \
		if (!(condition)) {                                                                         \
			::runtime::AbortAssertFailure(subsystem, step, #condition, detail, __FILE__, __LINE__); \
		}                                                                                           \
	} while (0)
#else
#define PV_ASSERT(condition, subsystem, step, detail) ((void)0)
#endif

