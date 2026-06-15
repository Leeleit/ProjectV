#pragma once

// **libc++ migration shim (`2026-06-13`).** libc++ 22 (the
// version shipped on Arch as of 2026-06-13) does not yet
// have `<inplace_vector>` (P0843, C++26). The stdlib
// feature is in libstdc++ 16.1.1 (we found it at
// `/usr/include/c++/16.1.1/inplace_vector`) but the
// libc++ patch is still in flight upstream as of
// `commit e0029dc`-era research.
//
// Until libc++ catches up, `core/Types.hpp` (which
// conditionally does `#include <inplace_vector>` only
// when the libstdc++ path resolves it — see
// `core/Types.hpp` line 51) falls back to this shim on
// libc++. The shim provides only the surface that the
// Tier 1.A call sites in
// `render/SceneResources.cpp::RebuildChunkVisibilityAndFillCache`
// / `ApplyCachedChunkVisibilityCommands` actually use:
// `size()`, `empty()`, `data()`, `operator[]`,
// `resize(N)` (value-init new slots). No `push_back` /
// `try_push_back` / `begin()` / `end()` are used (verified
// via `rg "opaqueCommands\.(begin|end|push|back|try_)"
// src/ tests/`).
//
// **Why not use `std::vector` instead?** The Tier 1.A
// commit `427be4f` picked `std::inplace_vector` over
// `std::vector` precisely because the fixed cap means
// the data pointer is stable (the `memcpy` from
// `cache.data()` to the per-frame mapped GPU buffer
// in `ApplyCachedChunkVisibilityCommands` cannot be
// invalidated by a later resize — for `std::vector` it
// could). The shim preserves that contract by using a
// `std::array` of `Capacity` `T`s plus an explicit
// `size_` field, which is exactly what the upstream
// `std::inplace_vector` does internally.

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace projectv::core {

template <typename T, std::size_t Capacity>
class InplaceVectorShim {
public:
	using value_type = T;
	using size_type = std::size_t;
	using iterator = T *;
	using const_iterator = const T *;

	static constexpr size_type capacity() noexcept { return Capacity; }
	static constexpr size_type max_size() noexcept { return Capacity; }

	constexpr InplaceVectorShim() noexcept = default;

	// **No copy/move ctors / assignment.** Matches
	// `std::inplace_vector`'s default (the C++26
	// spec makes it move-constructible when the
	// element type is; we don't need move for the
	// Tier 1.A use cases — `ChunkVisibilityCache`
	// lives inside `RenderState` which is moved by
	// pointer only).
	InplaceVectorShim(const InplaceVectorShim &) = delete;
	InplaceVectorShim &operator=(const InplaceVectorShim &) = delete;
	InplaceVectorShim(InplaceVectorShim &&) = delete;
	InplaceVectorShim &operator=(InplaceVectorShim &&) = delete;

	// **Element access.** `data()` returns a
	// stable pointer (no realloc possible — the
	// storage is the `std::array` member, not
	// heap).
	constexpr T *data() noexcept { return data_.data(); }
	constexpr const T *data() const noexcept { return data_.data(); }

	constexpr T &operator[](const size_type i) noexcept { return data_[i]; }
	constexpr const T &operator[](const size_type i) const noexcept { return data_[i]; }

	constexpr T &front() noexcept { return data_[0]; }
	constexpr const T &front() const noexcept { return data_[0]; }
	constexpr T &back() noexcept { return data_[size_ - 1]; }
	constexpr const T &back() const noexcept { return data_[size_ - 1]; }

	// **Size queries.** `size()` is the live
	// element count; `capacity()` is the static
	// max. The two differ after a `resize(N)` with
	// `N < Capacity`.
	constexpr size_type size() const noexcept { return size_; }
	constexpr bool empty() const noexcept { return size_ == 0; }
	constexpr bool full() const noexcept { return size_ == Capacity; }

	// **Resize.** Value-initialises new slots, drops
	// slots past the new size. Matches
	// `std::inplace_vector::resize(N)` semantics per
	// P0843 §3.7 ("If `n < size()`, the last `size() - n`
	// elements are removed; if `n > size()`, additional
	// default-inserted elements are appended").
	constexpr void resize(const size_type n) noexcept(std::is_nothrow_default_constructible_v<T>) {
		if (n > Capacity) {
			// **Pre-condition violation.** The
			// original `std::inplace_vector` has
			// the same contract (P0843 §3.7:
			// "Preconditions: `n <= capacity()`").
			// Caller in SceneResources.cpp has
			// `assert(chunkDescriptorCount <=
			// kChunkVisibilityCacheMaxChunks)`
			// right before the resize, so this
			// branch is unreachable in
			// release builds.
			return;
		}
		if (n > size_) {
			for (size_type i = size_; i < n; ++i) {
				data_[i] = T{};
			}
		}
		// `n < size_` shrinks the live count but
		// leaves the storage in place (the
		// `std::array` always has Capacity slots).
		size_ = n;
	}

	// **Clear.** Drops the live count to 0. The
	// storage is untouched (the `std::array`
	// always has Capacity slots).
	constexpr void clear() noexcept { size_ = 0; }

private:
	std::array<T, Capacity> data_{};
	size_type size_ = 0;
};

static_assert(std::is_trivially_copyable_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially copyable (std::array + size_t are both trivially copyable); the data pointer is therefore stable across moves, which is the Tier 1.A fixed-cap contract");
// **Non-trivially-default-constructible** is OK — the
// default ctor value-initialises the array, so a
// `ChunkVisibilityCache{}` member of `RenderState` is
// zero-initialised (the `size_` field is the in-class
// initialiser = 0, and the array's default ctor
// value-inits each `T`).
static_assert(std::is_trivially_destructible_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially destructible (no user-provided dtor)");

} // namespace projectv::core

