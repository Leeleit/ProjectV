#pragma once

#include <cstdlib>

#if defined(_WIN32)
#include <stdlib.h> // _putenv_s
#endif

namespace projectv::core {

// Wrapper around std::getenv. Kept as a function-pointer call so static analyzers
// cannot constant-fold the result, while remaining inline to avoid link issues.
inline const char *GetEnvVar(const char *const name)
{
	char *(*getter)(const char *) = std::getenv;
	return getter(name);
}

inline int SetEnvVar(const char *const name, const char *const value, const int overwrite = 1)
{
#if defined(_WIN32)
	if (overwrite == 0 && std::getenv(name) != nullptr) {
		return 0;
	}
	return _putenv_s(name, value != nullptr ? value : "");
#else
	return ::setenv(name, value, overwrite);
#endif
}

inline int UnsetEnvVar(const char *const name)
{
#if defined(_WIN32)
	return _putenv_s(name, "");
#else
	return ::unsetenv(name);
#endif
}

} // namespace projectv::core
