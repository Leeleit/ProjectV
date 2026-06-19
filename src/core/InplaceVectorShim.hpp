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


	[[nodiscard]] constexpr T *data() noexcept { return data_.data(); }
	[[nodiscard]] constexpr const T *data() const noexcept { return data_.data(); }

	[[nodiscard]] constexpr T &operator[](const size_type i) noexcept { return data_[i]; }
	[[nodiscard]] constexpr const T &operator[](const size_type i) const noexcept { return data_[i]; }

	[[nodiscard]] constexpr T &front() noexcept { return data_[0]; }
	[[nodiscard]] constexpr const T &front() const noexcept { return data_[0]; }
	[[nodiscard]] constexpr T &back() noexcept { return data_[size_ - 1]; }
	[[nodiscard]] constexpr const T &back() const noexcept { return data_[size_ - 1]; }


	[[nodiscard]] constexpr size_type size() const noexcept { return size_; }
	[[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
	[[nodiscard]] constexpr bool full() const noexcept { return size_ == Capacity; }


	constexpr void resize(const size_type n) noexcept(std::is_nothrow_default_constructible_v<T>) {
		if (n > Capacity) {

			return;
		}
		if (n > size_) {
			for (size_type i = size_; i < n; ++i) {
				data_[i] = T{};
			}
		}

		size_ = n;
	}


	constexpr void clear() noexcept { size_ = 0; }

private:
	std::array<T, Capacity> data_{};
	size_type size_ = 0;
};

static_assert(std::is_trivially_copyable_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially copyable (std::array + size_t are both trivially copyable); the data pointer is therefore stable across moves, which is the Tier 1.A fixed-cap contract");

static_assert(std::is_trivially_destructible_v<InplaceVectorShim<int, 4>> == true,
	"InplaceVectorShim must be trivially destructible (no user-provided dtor)");

} // namespace projectv::core

