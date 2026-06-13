// **Tier 4 (`2026-06-13`).** C++ wrapper for the
// `c_kernels/frustum_cull` C kernel. Two entry points:
//
// - `FilterVisibleInstances` — takes the engine's
//   `std::span<const ModelInstanceData>` + a
//   `ChunkCullingParameters`, returns a filtered
//   `std::vector<ModelInstanceData>` of the AABBs
//   visible against the camera frustum. Replaces
//   the per-AABB `IsAabbVisibleAgainstCameraFrustum`
//   call in `BuildVisibleModelInstanceList` with a
//   single batched C kernel call.
//
// - `CullVisibleMask` — returns the raw 8-bits-per-byte
//   visible mask, useful for diagnostics and tests.
//
// **Why a C++ wrapper, not direct C call from the
// engine TU.** The engine stays in C++ land; the C
// ABI is a single linkage seam. The wrapper owns the
// temporary `ProjectvCAabb` conversion buffer and the
// output mask buffer, and re-uses them across calls
// (per-frame work happens in a `RenderState` that
// outlives the kernel call).
//
// **Why the scalar C kernel, not the AVX2 one.** Per
// the Tier 3 benchmark (see
// `src/bench/FrustumCullBenchmark.cpp`): on this
// hardware + the AoS `ProjectvCAabb` layout, the
// scalar C kernel is 3.7-3.9× faster than the C++
// math:: baseline; the AVX2 kernel is 2.5-2.7× (the
// autovectorizer with `-mavx2` at the consumer side
// beats the hand-rolled `_mm256_setr_ps` setup in
// debug builds). The AVX2 path stays in the tree as
// the future optimisation when the AABB data is
// reorganised into SoA layout — at that point the
// intrinsics path is expected to reach the 8× target
// from `agent/memory.md §1583`.
//
// **Threading.** Not thread-safe — the temporary
// buffers are member state, not TLS. The engine calls
// this from the render thread, which is the only
// caller; if a future frame graph parallelises the
// cull, the buffers need to move to thread-local or
// per-frame allocation.
#ifndef PROJECTV_C_KERNELS_FRUSTUM_CULLING_HPP
#define PROJECTV_C_KERNELS_FRUSTUM_CULLING_HPP

#include "c_kernels/frustum_cull.hpp"
#include "core/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace projectv::c_kernels {

// **Per-call conversion helper.** Translates the
// engine's `ChunkCullingParameters` (Vec4 / Vec3
// fields) into the C kernel's `ProjectvCFrustumCullParameters`
// POD. The `w` components are unpacked into the
// scalar fields (maxDistance, tanHalfVerticalFov, etc.)
// the kernel reads. The `tanHalfVerticalFov` etc. are
// clamped to >= 0 here to mirror the
// `std::max(parameters.*.w, 0.0f)` clamps in the
// C++ helper.
[[nodiscard]] ProjectvCFrustumCullParameters ToCParameters(
	const ChunkCullingParameters &parameters) noexcept;

// **Culls `instances` against `parameters` in a single
// batched C kernel call.** Returns the filtered list
// (only the AABBs visible against the camera frustum).
//
// The input span is taken by `const` reference; the
// returned vector is freshly allocated. Per-frame
// call cost is dominated by the conversion to
// `ProjectvCAabb` (~1 µs for 300 instances) and the
// kernel itself (~50 µs for 300 instances per the
// Tier 3 benchmark); the resulting filtered vector
// is a copy of the source AABB data.
//
// **When NOT to use this.** For `count < 8`, the
// C kernel's per-batch setup is wasted work. The
// engine should fall back to the inline
// `IsAabbVisibleAgainstCameraFrustum` helper for
// very small input lists. The threshold is
// `kBatchDispatchThreshold = 8`.
[[nodiscard]] std::vector<ModelInstanceData> FilterVisibleInstances(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);

// **Lower-level entry point.** Returns the raw
// 8-bits-per-byte visible mask (bit i of byte i/8
// is set iff AABB i is visible). The output vector
// is `(count + 7) / 8` bytes long, allocated
// freshly per call. Used by `FilterVisibleInstances`
// and by the unit test
// `ProjectVCFrustumCullingTest` (forthcoming).
[[nodiscard]] std::vector<uint8_t> CullVisibleMask(
	const std::span<const ModelInstanceData> &instances,
	const ChunkCullingParameters &parameters);

// **Crossover threshold.** Below this many AABBs,
// the C kernel's per-batch setup cost is not
// amortised. Engine code should fall back to the
// inline `IsAabbVisibleAgainstCameraFrustum`
// helper for `instances.size() < kBatchDispatchThreshold`.
// The 8-AABB threshold matches the kernel's lane
// width (the first full AVX2 batch covers exactly
// 8 AABBs).
constexpr std::size_t kBatchDispatchThreshold = 8;

} // namespace projectv::c_kernels

#endif
