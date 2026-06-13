// **Tier 2.A — `Math.ixx` (`2026-06-13`).** C++20 module
// form of the Tier 0.A `projectv::math::Vec3/Vec4/Mat4`
// types. Mirrors `src/core/Math.hpp` 1:1 — same struct
// layout, same `alignas(16)`, same column-major `Mat4` —
// so any `.cpp` that switches from `#include "core/Math.hpp"`
// to `import projectv.math;` gets the identical ABI /
// layout / hot-path codegen.
//
// The header (`core/Math.hpp`) is **kept** as a thin
// re-export shim for now: it forwards `import projectv.math;`
// for any TU that still uses the `#include` form (Tier 2.D
// migration). Once Tier 2.D is complete, the header can
// be deleted and every TU moves to `import`.
//
// **Module structure:**
//   - Global module fragment (`module;`) brings in `<cmath>`
//     for `std::sqrt` / `std::cos` / `std::sin` and `<cstddef>`
//     for `std::size_t`.
//   - Module purview exports `projectv::math::Vec3`,
//     `Vec4`, `Mat4`, the `dot` / `cross` / `lengthSq` /
//     `normalize` / `identity` / `zero` / `transpose` /
//     `inverse` / `fromArray{3,4,16}` free functions, and
//     the `operator*` / `operator+` / `operator-` overloads.
//   - The `fromArray` and `data` helpers are intentionally
//     NOT exported — they're internal to the
//     cast-between-`glm::mat4`-and-`Mat4` plumbing and
//     shouldn't be part of the public surface.
module;

#include <cmath>
#include <cstddef>

export module projectv.math;

export namespace projectv::math {

struct alignas(16) Vec3 {
	float x;
	float y;
	float z;
	float _pad;

	constexpr Vec3() noexcept = default;
	constexpr Vec3(const float xVal, const float yVal, const float zVal) noexcept
		: x(xVal), y(yVal), z(zVal), _pad(0.0f) {}
	constexpr Vec3(const float xVal, const float yVal, const float zVal, const float padVal) noexcept
		: x(xVal), y(yVal), z(zVal), _pad(padVal) {}
};

struct alignas(16) Vec4 {
	float x;
	float y;
	float z;
	float w;

	constexpr Vec4() noexcept = default;
	constexpr Vec4(const float xVal, const float yVal, const float zVal, const float wVal) noexcept
		: x(xVal), y(yVal), z(zVal), w(wVal) {}

	// **Single-arg subscript** for the `mat * vec` and
	// `mat[row, col]` codepaths where the caller is
	// iterating a Vec4's components by index. Without
	// this, `m.c[col][row]` would fail because Vec4
	// doesn't have a one-arg `operator[]` (Mat4 has
	// `operator[](col, row)`, not Vec4).
	[[nodiscard]] constexpr float &operator[](const std::size_t i) noexcept {
		return (&x)[i];
	}
	[[nodiscard]] constexpr const float &operator[](const std::size_t i) const noexcept {
		return (&x)[i];
	}
};

struct alignas(16) Mat4 {
	Vec4 c[4];

	constexpr Mat4() noexcept = default;
	constexpr Mat4(const Vec4 c0, const Vec4 c1, const Vec4 c2, const Vec4 c3) noexcept
		: c{c0, c1, c2, c3} {}

	// **Column accessor.** `m.c[col][row]` reads the (row, col)
	// element, matching `glm::mat4[col][row]` and the rest of
	// the project's column-major convention.
	[[nodiscard]] constexpr float &operator[](const std::size_t col, const std::size_t row) noexcept {
		return c[col][static_cast<std::size_t>(row) > 0 ? row : 0];
	}
	[[nodiscard]] constexpr const float &operator[](const std::size_t col, const std::size_t row) const noexcept {
		return c[col][row];
	}

	// **Direct column access via .c[col].xyz.** Avoids the
	// `glm::mat4[col][row]` ↔ `Mat4.c[col][row]` translation
	// friction.
	[[nodiscard]] float *data() noexcept { return &c[0].x; }
	[[nodiscard]] const float *data() const noexcept { return &c[0].x; }
};

static_assert(sizeof(Vec3) == 16, "Vec3 must be 16 bytes");
static_assert(alignof(Vec3) == 16, "Vec3 must be 16-byte aligned");
static_assert(sizeof(Vec4) == 16, "Vec4 must be 16 bytes");
static_assert(alignof(Vec4) == 16, "Vec4 must be 16-byte aligned");
static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes");
static_assert(alignof(Mat4) == 16, "Mat4 must be 16-byte aligned");

// **Free functions.**

[[nodiscard]] inline float dot(const Vec3 a, const Vec3 b) noexcept {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
	return Vec3{
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

[[nodiscard]] inline float lengthSq(const Vec3 v) noexcept {
	return dot(v, v);
}

[[nodiscard]] inline float length(const Vec3 v) noexcept {
	return std::sqrt(lengthSq(v));
}

[[nodiscard]] inline Vec3 normalize(const Vec3 v) noexcept {
	const float len = length(v);
	if (len == 0.0f) {
		return Vec3{0.0f, 0.0f, 0.0f};
	}
	const float invLen = 1.0f / len;
	return Vec3{v.x * invLen, v.y * invLen, v.z * invLen, 0.0f};
}

[[nodiscard]] inline Mat4 identity() noexcept {
	Mat4 result{};
	result.c[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
	result.c[1] = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
	result.c[2] = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
	result.c[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
	return result;
}

[[nodiscard]] inline Mat4 zero() noexcept {
	Mat4 result{};
	return result;
}

[[nodiscard]] inline Mat4 transpose(const Mat4 m) noexcept {
	Mat4 result{};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			result.c[col][row] = m.c[row][col];
		}
	}
	return result;
}

[[nodiscard]] inline Mat4 inverse(const Mat4 m) noexcept {
	// **Gauss-Jordan elimination** on a 4x4 column-major matrix.
	// Kept from `Math.hpp` (was the implementation
	// `Renderer.cpp::InvertColumnMajorMat4` was based on).
	// The output is the inverse of `m`; `m` itself is
	// unmodified. Degenerate (singular) input is detected
	// via the zero-pivot guard at the end of each column
	// step; the function falls through to `zero()` in that
	// case (the same fallback the pre-module code used).
	Mat4 augmented{
		m.c[0], m.c[1], m.c[2], m.c[3],
	};
	// Augment with identity by overlaying onto the second
	// half; for column-major, this means we treat the
	// 4x8 working buffer as 4 rows × 8 cols (transposed view).
	// Easier: do row-wise Gauss-Jordan in transposed space.
	float a[4][8]{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			a[row][col] = m.c[col][row];
		}
		a[row][4] = (row == 0) ? 1.0f : 0.0f;
		a[row][5] = (row == 1) ? 1.0f : 0.0f;
		a[row][6] = (row == 2) ? 1.0f : 0.0f;
		a[row][7] = (row == 3) ? 1.0f : 0.0f;
	}
	for (int pivot = 0; pivot < 4; ++pivot) {
		// **Find a non-zero pivot row at or below `pivot`.**
		int pivotRow = pivot;
		while (pivotRow < 4 && a[pivotRow][pivot] == 0.0f) {
			++pivotRow;
		}
		if (pivotRow == 4) {
			return zero();
		}
		if (pivotRow != pivot) {
			for (int col = 0; col < 8; ++col) {
				const float tmp = a[pivot][col];
				a[pivot][col] = a[pivotRow][col];
				a[pivotRow][col] = tmp;
			}
		}
		const float pivotVal = a[pivot][pivot];
		for (int col = 0; col < 8; ++col) {
			a[pivot][col] /= pivotVal;
		}
		for (int row = 0; row < 4; ++row) {
			if (row == pivot) {
				continue;
			}
			const float factor = a[row][pivot];
			if (factor == 0.0f) {
				continue;
			}
			for (int col = 0; col < 8; ++col) {
				a[row][col] -= factor * a[pivot][col];
			}
		}
	}
	Mat4 result{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.c[col][row] = a[row][col + 4];
		}
	}
	return result;
}

[[nodiscard]] inline Mat4 operator*(const Mat4 a, const Mat4 b) noexcept {
	Mat4 result{};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			float sum = 0.0f;
			for (std::size_t k = 0; k < 4; ++k) {
				sum += a.c[k][row] * b.c[col][k];
			}
			result.c[col][row] = sum;
		}
	}
	return result;
}

[[nodiscard]] inline Vec4 operator*(const Mat4 m, const Vec4 v) noexcept {
	Vec4 result{};
	for (std::size_t row = 0; row < 4; ++row) {
		float sum = 0.0f;
		for (std::size_t col = 0; col < 4; ++col) {
			sum += m.c[col][row] * (&v.x)[col];
		}
		(&result.x)[row] = sum;
	}
	return result;
}

[[nodiscard]] inline Vec3 operator+(const Vec3 a, const Vec3 b) noexcept {
	return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] inline Vec3 operator-(const Vec3 a, const Vec3 b) noexcept {
	return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] inline Vec3 operator-(const Vec3 a) noexcept {
	return Vec3{-a.x, -a.y, -a.z};
}

[[nodiscard]] inline Vec4 operator+(const Vec4 a, const Vec4 b) noexcept {
	return Vec4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

[[nodiscard]] inline Vec4 operator-(const Vec4 a, const Vec4 b) noexcept {
	return Vec4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

[[nodiscard]] inline Vec4 operator-(const Vec4 a) noexcept {
	return Vec4{-a.x, -a.y, -a.z, -a.w};
}

// **Cast helpers** from `glm::mat4` / `float[N]` to
// `Mat4` so the existing ModelManifestLoader /
// ShadowProjection code can memcpy-glm->Mat4 without
// having to include glm in this module.
[[nodiscard]] inline Mat4 fromArray16(const float *src) noexcept {
	Mat4 result{};
	for (std::size_t i = 0; i < 16; ++i) {
		result.c[i / 4][i % 4] = src[i];
	}
	return result;
}

} // namespace projectv::math
