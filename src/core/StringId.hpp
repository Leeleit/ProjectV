#pragma once

#if defined(__clang__) && defined(_MSC_VER)
#include "core/StringId_fallback.hpp"
#else
import projectv.string_id;
#endif

