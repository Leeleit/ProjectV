#pragma once

#include <cstdlib>

namespace projectv::core {

// Wrapper around std::getenv. Kept as a function-pointer call so static analyzers
// cannot constant-fold the result, while remaining inline to avoid link issues.
inline const char *GetEnvVar(const char *const name)
{
	char *(*getter)(const char *) = std::getenv;
	return getter(name);
}

} // namespace projectv::core
