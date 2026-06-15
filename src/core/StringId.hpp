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
import projectv.string_id;

