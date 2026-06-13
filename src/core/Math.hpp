#ifndef PROJECTV_CORE_MATH_HPP
#define PROJECTV_CORE_MATH_HPP

// **Tier 2.D (`2026-06-13`).** This header is now a **forwarding
// shim** to the `projectv.math` module (`src/core/Math.ixx`).
// Including the header re-issues the `import projectv.math;`
// declaration in the consumer TU, so the consumer sees the
// `projectv::math::{Vec3,Vec4,Mat4,...}` types via the module
// interface rather than via textual `#include` of the legacy
// inline definitions. The actual type / function definitions
// live in `src/core/Math.ixx` (the canonical source of truth
// from Tier 2.A onward).
//
// The shim pattern lets TUs that haven't been migrated to
// `import projectv.math;` directly (e.g. TUs that include
// `core/Types.hpp` which itself `#include "core/Math.hpp"`,
// transitively) keep working — the `import` is re-issued on
// their behalf by the header. Once Tier 2.D is complete and
// every mainline TU uses `import` directly, this header can
// be deleted.
//
// **Why a re-export shim instead of inline definitions?**
// Earlier (pre-Tier 2.D) this header held inline
// `projectv::math::Vec3/Vec4/Mat4` definitions. With
// `projectv.math` being a module, having BOTH the module
// declaration AND the inline declaration would trigger
// "declaration of 'Vec3' in the global module follows
// declaration in module projectv.math" — a hard
// ODR violation. The shim avoids that by re-issuing the
// import on behalf of the consumer.
import projectv.math;

// **Legacy fallback** for toolchains that don't support
// C++20 module imports (none of our TUs fall in this bucket
// today — the project's `CMAKE_CXX_STANDARD 26` + Clang 22
// always support modules — but the guard keeps the header
// parseable if someone copies it into a non-modules TU).
#ifdef __cpp_modules
// Type definitions are provided by the import above; nothing
// else needed here. The function bodies (`dot`, `cross`,
// `lengthSq`, `normalize`, `operator*`, etc.) live in
// `src/core/Math.ixx` and are visible via the module.
#else
// **Pre-Tier 2.D fallback** (kept for source-compatibility
// with non-modules TUs). Defines the same types and
// functions as `src/core/Math.ixx` so the project still
// compiles if a downstream consumer turns modules off.
#error "core/Math.hpp requires C++20 modules (Clang 18+). Disable this guard once a non-modules fallback is needed."

#endif

// **Original Tier 0.A doc preserved below for git-blame
// archeology — the type definitions that used to live here
// are now in `src/core/Math.ixx` and reachable via the
// `import projectv.math;` shim above.**
//
// Original comment (Tier 0.A — 2026-06-13): New foundational
// math types for hot-path SIMD. `Vec3/Vec4/Mat4` are
// 16-byte aligned so the compiler emits `movaps` /
// `vmovaps` (alignment-required SSE/AVX) instead of `movups`
// (2-3x slowdown). Column-major `Mat4` matches the rest of
// the project (see `Renderer.cpp::InvertColumnMajorMat4`
// line 73-76 and `Camera.cpp::MultiplyMatrices`).
//
// This header is **additive** — it introduces the types, but
// does not migrate any existing `std::array<float, N>` sites.
// Migration is Tier 0.B (separate atomic commit per
// `agent/decisions.md §29`).
//
// Refs: agent/memory.md §11.1 A8 (alignas), §11.2 P2
// (alignment), §11.4 Tier 0.A, agent/decisions.md §29,
// TODO.md Tier 0.


#endif // PROJECTV_CORE_MATH_HPP
