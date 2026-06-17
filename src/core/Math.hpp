#pragma once

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
//
// **Windows clang-cl fallback (`2026-06-18`,
// windows-host-build-r0).** CMake 4.2's `FILE_SET CXX_MODULES`
// driver requires a module scanner, and clang-cl 22 does
// not provide one. To keep the Windows host build green
// without forking the mainline math type definitions,
// the Windows clang-cl branch pulls in
// `Math_fallback.hpp` (a header-only duplicate of the
// `projectv.math` module). The branch condition
// `defined(__clang__) && defined(_MSC_VER)` matches
// clang-cl specifically; native clang on Linux/macOS and
// pure MSVC cl.exe keep consuming the `projectv.math`
// module. When CMake or clang-cl ship the missing scanner,
// this branch drops and `Math_fallback.hpp` becomes dead
// code (delete both).
#if defined(__clang__) && defined(_MSC_VER)
#include "core/Math_fallback.hpp"
#else
import projectv.math;
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


