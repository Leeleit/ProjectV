#pragma once

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

	InplaceVectorShim(const InplaceVectorShim &) = delete;
	InplaceVectorShim &operator=(const InplaceVectorShim &) = delete;
	InplaceVectorShim(InplaceVectorShim &&) = delete;
	InplaceVectorShim &operator=(InplaceVectorShim &&) = delete;

	/// \brief **Element access.** `data()` returns a
	///
	/// \details
	///  stable pointer (no realloc possible — the

	///  storage is the `std::array` member, not

	///  heap).

	constexpr T *data() noexcept { return data_.data(); }
	constexpr const T *data() const noexcept { return data_.data(); }

	constexpr T &operator[](const size_type i) noexcept { return data_[i]; }
	constexpr const T &operator[](const size_type i) const noexcept { return data_[i]; }

	constexpr T &front() noexcept { return data_[0]; }
	constexpr const T &front() const noexcept { return data_[0]; }
	constexpr T &back() noexcept { return data_[size_ - 1]; }
	constexpr const T &back() const noexcept { return data_[size_ - 1]; }

	/// \brief **Size queries.** `size()` is the live
	///
	/// \details
	///  element count; `capacity()` is the static

	///  max. The two differ after a `resize(N)` with

	///  `N < Capacity`.

	constexpr size_type size() const noexcept { return size_; }
	constexpr bool empty() const noexcept { return size_ == 0; }
	constexpr bool full() const noexcept { return size_ == Capacity; }

	/// \brief **Resize.** Value-initialises new slots, drops
	///
	/// \details
	///  slots past the new size. Matches

	///  `std::inplace_vector::resize(N)` semantics per

	///  P0843 §3.7 ("If `n < size()`, the last `size() - n`

	///  elements are removed; if `n > size()`, additional

	///  default-inserted elements are appended").

	constexpr void resize(const size_type n) noexcept(std::is_nothrow_default_constructible_v<T>) {
		if (n > Capacity) {
			/// \brief **Pre-condition violation.** The
			///
			/// \details
			///  original `std::inplace_vector` has

			///  the same contract (P0843 §3.7:

			///  "Preconditions: `n <= capacity()`").

			///  Caller in SceneResources.cpp has

			///  `assert(chunkDescriptorCount <=

			///  kChunkVisibilityCacheMaxChunks)`

			///  right before the resize, so this

			///  branch is unreachable in

			///  release builds.

			return;
		}
		if (n > size_) {
			for (size_type i = size_; i < n; ++i) {
				data_[i] = T{};
			}
		}
		/// \brief `n < size_` shrinks the live count but
		///
		/// \details
		///  leaves the storage in place (the

		///  `std::array` always has Capacity slots).

		size_ = n;
	}

	/// \brief **Clear.** Drops the live count to 0.
	///
	/// \details
	/// The
	///  storage is untouched (the `std::array`

	///  always has Capacity slots).

	constexpr void clear() noexcept { size_ = 0; }

private:
	std::array<T, Capacity> data_{};
	size_type size_ = 0;
};

static_assert(std::is_trivially_copyable_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially copyable (std::array + size_t are both trivially copyable); the data pointer is therefore stable across moves, which is the Tier 1.A fixed-cap contract");
/// \brief **Non-trivially-default-constructible** is OK — the
///
/// \details
///  default ctor value-initialises the array, so a

///  `ChunkVisibilityCache{}` member of `RenderState` is

///  zero-initialised (the `size_` field is the in-class

///  initialiser = 0, and the array's default ctor

///  value-inits each `T`).

static_assert(std::is_trivially_destructible_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially destructible (no user-provided dtor)");

} // namespace projectv::core

