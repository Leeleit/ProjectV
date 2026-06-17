#pragma once

// **Tier 2.D (`2026-06-13`).** This header is a **forwarding
// shim** to the `projectv.string_id` module
// (`src/core/StringId.ixx`). The actual type / function
// definitions live in the module; the shim re-issues the
// `import` on behalf of consumers that still rely on the
// textual `#include` form. Same pattern as `core/Math.hpp`
// (commit `73e2dd7`). Per the comment in `Math.hpp`, once
// every mainline TU is a direct importer of the module,
// both shims can be deleted.
//
// **Windows clang-cl fallback (`2026-06-18`,
// windows-host-build-r0).** Mirrors the `Math.hpp`
// branch: clang-cl on Windows pulls in
// `StringId_fallback.hpp` (a header-only duplicate of
// `projectv.string_id`) instead of the module.
#if defined(__clang__) && defined(_MSC_VER)
#include "core/StringId_fallback.hpp"
#else
import projectv.string_id;
#endif

