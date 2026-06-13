#ifndef PROJECTV_CORE_MATH_HPP
#define PROJECTV_CORE_MATH_HPP

// **Tier 0.A — r0 hardcore perf (`2026-06-13`).** New foundational
// math types for hot-path SIMD. `Vec3/Vec4/Mat4` are 16-byte aligned
// so the compiler emits `movaps` / `vmovaps` (alignment-required
// SSE/AVX) instead of `movups` (2-3x slowdown). Column-major
// `Mat4` matches the rest of the project (see
// `Renderer.cpp::InvertColumnMajorMat4` line 73-76 and
// `Camera.cpp::MultiplyMatrices`).
//
// This header is **additive** — it introduces the types, but does
// not migrate any existing `std::array<float, N>` sites. Migration
// is Tier 0.B (separate atomic commit per `agent/decisions.md §29`).
//
// Refs: agent/memory.md §11.1 A8 (alignas), §11.2 P2 (alignment),
// §11.4 Tier 0.A, agent/decisions.md §29, TODO.md Tier 0.

#include <array>
#include <cmath>
#include <cstddef>

namespace projectv::math {

struct alignas(16) Vec3 {
	// **Explicit `_pad` field** instead of relying on implicit struct
	// tail padding, so brace-init `Vec3{1.0f, 2.0f, 3.0f, 0.0f}` is
	// valid aggregate init. The default member initializer also
	// guarantees zero-init for `Vec3{}` / `Vec3 a;`.
	float x;
	float y;
	float z;
	float _pad = 0.0f;

	constexpr float& operator[](const std::size_t i) noexcept
	{
		return (&x)[i];
	}
	constexpr float operator[](const std::size_t i) const noexcept
	{
		return (&x)[i];
	}

	constexpr float* data() noexcept
	{
		return &x;
	}
	constexpr const float* data() const noexcept
	{
		return &x;
	}
};
static_assert(sizeof(Vec3) == 16, "Vec3 must be 16 bytes (4 floats, 1 padding to 16-byte alignment)");
static_assert(alignof(Vec3) == 16, "Vec3 must be 16-byte aligned for SIMD movaps");

struct alignas(16) Vec4 {
	float x;
	float y;
	float z;
	float w;

	constexpr float& operator[](const std::size_t i) noexcept
	{
		return (&x)[i];
	}
	constexpr float operator[](const std::size_t i) const noexcept
	{
		return (&x)[i];
	}

	constexpr float* data() noexcept
	{
		return &x;
	}
	constexpr const float* data() const noexcept
	{
		return &x;
	}
};
static_assert(sizeof(Vec4) == 16, "Vec4 must be 16 bytes");
static_assert(alignof(Vec4) == 16, "Vec4 must be 16-byte aligned for SIMD");

// **Column-major Mat4** — `c[i]` is column i (0..3). Indexing:
// `m[col][row]` or `data()[col*4 + row]`. Matches the rest of the
// project (`Renderer.cpp::InvertColumnMajorMat4`, `Camera.cpp`).
struct alignas(16) Mat4 {
	Vec4 c[4];

	constexpr Vec4& column(const std::size_t i) noexcept
	{
		return c[i];
	}
	constexpr Vec4 column(const std::size_t i) const noexcept
	{
		return c[i];
	}

	constexpr float& m(const std::size_t col, const std::size_t row) noexcept
	{
		return c[col][row];
	}
	constexpr float m(const std::size_t col, const std::size_t row) const noexcept
	{
		return c[col][row];
	}

	constexpr float* data() noexcept
	{
		return c[0].data();
	}
	constexpr const float* data() const noexcept
	{
		return c[0].data();
	}
};
static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes (4 columns x 16 bytes)");
static_assert(alignof(Mat4) == 16, "Mat4 must be 16-byte aligned for SIMD");

// EVIL: scalar sqrt/fabs — auto-vectorizer can't fuse these with
// the surrounding load/store for most loop shapes. C++26
// `std::simd<float, 4>` would replace them; Clang 22 supports it
// (Tier 4 / R&D per agent/decisions.md §29).
inline float lengthSq(const Vec3 v) noexcept
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float lengthSq(const Vec4 v) noexcept
{
	return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

inline float length(const Vec3 v) noexcept
{
	return std::sqrt(lengthSq(v));
}

inline float length(const Vec4 v) noexcept
{
	return std::sqrt(lengthSq(v));
}

inline float dot(const Vec3 a, const Vec3 b) noexcept
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float dot(const Vec4 a, const Vec4 b) noexcept
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept
{
	return Vec3{
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
		0.0f,
	};
}

inline Vec3 normalize(const Vec3 v) noexcept
{
	const float lenSq = lengthSq(v);
	if (lenSq <= 0.0f) {
		return Vec3{0.0f, 0.0f, 0.0f, 0.0f};
	}
	const float invLen = 1.0f / std::sqrt(lenSq);
	return Vec3{v.x * invLen, v.y * invLen, v.z * invLen, 0.0f};
}

inline Vec4 normalize(const Vec4 v) noexcept
{
	const float lenSq = lengthSq(v);
	if (lenSq <= 0.0f) {
		return Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	}
	const float invLen = 1.0f / std::sqrt(lenSq);
	return Vec4{v.x * invLen, v.y * invLen, v.z * invLen, v.w * invLen};
}

inline Vec3 operator+(const Vec3 a, const Vec3 b) noexcept
{
	return Vec3{a.x + b.x, a.y + b.y, a.z + b.z, 0.0f};
}

inline Vec3 operator-(const Vec3 a, const Vec3 b) noexcept
{
	return Vec3{a.x - b.x, a.y - b.y, a.z - b.z, 0.0f};
}

inline Vec3 operator-(const Vec3 a) noexcept
{
	return Vec3{-a.x, -a.y, -a.z, 0.0f};
}

inline Vec3 operator*(const Vec3 a, const float s) noexcept
{
	return Vec3{a.x * s, a.y * s, a.z * s, 0.0f};
}

inline Vec3 operator*(const float s, const Vec3 a) noexcept
{
	return Vec3{a.x * s, a.y * s, a.z * s, 0.0f};
}

inline Vec3 operator/(const Vec3 a, const float s) noexcept
{
	const float inv = 1.0f / s;
	return Vec3{a.x * inv, a.y * inv, a.z * inv, 0.0f};
}

inline Vec4 operator+(const Vec4 a, const Vec4 b) noexcept
{
	return Vec4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

inline Vec4 operator-(const Vec4 a, const Vec4 b) noexcept
{
	return Vec4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

inline Vec4 operator-(const Vec4 a) noexcept
{
	return Vec4{-a.x, -a.y, -a.z, -a.w};
}

inline Vec4 operator*(const Vec4 a, const float s) noexcept
{
	return Vec4{a.x * s, a.y * s, a.z * s, a.w * s};
}

inline Vec4 operator*(const float s, const Vec4 a) noexcept
{
	return Vec4{a.x * s, a.y * s, a.z * s, a.w * s};
}

inline Vec4 operator/(const Vec4 a, const float s) noexcept
{
	const float inv = 1.0f / s;
	return Vec4{a.x * inv, a.y * inv, a.z * inv, a.w * inv};
}

inline Vec4 operator*(const Mat4 m, const Vec4 v) noexcept
{
	// column-major mat * vec: out[col] = sum_row(m[row][col] * v[row])
	// in column-major storage that's out[col] = m.c[col].dot(v).
	return Vec4{
		m.c[0].x * v.x + m.c[1].x * v.y + m.c[2].x * v.z + m.c[3].x * v.w,
		m.c[0].y * v.x + m.c[1].y * v.y + m.c[2].y * v.z + m.c[3].y * v.w,
		m.c[0].z * v.x + m.c[1].z * v.y + m.c[2].z * v.z + m.c[3].z * v.w,
		m.c[0].w * v.x + m.c[1].w * v.y + m.c[2].w * v.z + m.c[3].w * v.w,
	};
}

inline Mat4 operator*(const Mat4 a, const Mat4 b) noexcept
{
	Mat4 out{};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			out.m(col, row) = a.m(0, row) * b.m(col, 0)
							+ a.m(1, row) * b.m(col, 1)
							+ a.m(2, row) * b.m(col, 2)
							+ a.m(3, row) * b.m(col, 3);
		}
	}
	return out;
}

inline Mat4 transpose(const Mat4 m) noexcept
{
	Mat4 out{};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			out.m(col, row) = m.m(row, col);
		}
	}
	return out;
}

// 4x4 matrix inverse via Gauss-Jordan elimination with partial
// pivoting. Column-major layout, same convention as
// `Renderer.cpp::InvertColumnMajorMat4` (which this replaces in
// Tier 0.B). Used at most once per frame (TAA resolve), so the
// scalar cost is irrelevant. The check that `det != 0` would be a
// real concern for a singular matrix, but the projection matrix
// produced by `BuildGraphicsPushConstants` is non-singular for any
// sensible near/far pair.
inline Mat4 inverse(const Mat4 m) noexcept
{
	Mat4 augmented = m;
	Mat4 inverseMat{};
	inverseMat.c[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
	inverseMat.c[1] = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
	inverseMat.c[2] = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
	inverseMat.c[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};

	for (std::size_t pivot = 0; pivot < 4; ++pivot) {
		std::size_t bestRow = pivot;
		float bestAbs = std::fabs(augmented.m(pivot, pivot));
		for (std::size_t row = pivot + 1; row < 4; ++row) {
			const float candidateAbs = std::fabs(augmented.m(row, pivot));
			if (candidateAbs > bestAbs) {
				bestAbs = candidateAbs;
				bestRow = row;
			}
		}
		if (bestRow != pivot) {
			const Vec4 tmpAugCol = augmented.c[pivot];
			augmented.c[pivot] = augmented.c[bestRow];
			augmented.c[bestRow] = tmpAugCol;
			const Vec4 tmpInvCol = inverseMat.c[pivot];
			inverseMat.c[pivot] = inverseMat.c[bestRow];
			inverseMat.c[bestRow] = tmpInvCol;
		}
		const float pivotValue = augmented.m(pivot, pivot);
		if (pivotValue == 0.0f) {
			// Singular matrix; downstream consumer would produce undefined
			// output. Per `Renderer.cpp::InvertColumnMajorMat4` line 110-118
			// the previous code returned the partial-inverse state with a
			// comment saying the TAA-on path is gated off so this branch is
			// unreachable in mainline.
			return inverseMat;
		}
		const float invPivot = 1.0f / pivotValue;
		augmented.c[pivot] = augmented.c[pivot] * invPivot;
		inverseMat.c[pivot] = inverseMat.c[pivot] * invPivot;

		for (std::size_t row = 0; row < 4; ++row) {
			if (row == pivot) {
				continue;
			}
			const float factor = augmented.m(row, pivot);
			if (factor == 0.0f) {
				continue;
			}
			augmented.c[row] = augmented.c[row] - augmented.c[pivot] * factor;
			inverseMat.c[row] = inverseMat.c[row] - inverseMat.c[pivot] * factor;
		}
	}
	return inverseMat;
}

inline constexpr Mat4 identity() noexcept
{
	Mat4 out{};
	out.c[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
	out.c[1] = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
	out.c[2] = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
	out.c[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
	return out;
}

inline constexpr Mat4 zero() noexcept
{
	Mat4 out{};
	out.c[0] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	out.c[1] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	out.c[2] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	out.c[3] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	return out;
}

// **Cast helpers** — hot-path migration (Tier 0.B) needs to convert
// between `std::array<float, N>` (existing sites) and the new
// types without per-call explicit construction. These are
// `constexpr` so a `static Vec3 fromArray(...)` at namespace scope
// inlines to the same code as a direct initializer.
constexpr Vec3 fromArray3(const std::array<float, 3> a) noexcept
{
	return Vec3{a[0], a[1], a[2], 0.0f};
}

constexpr Vec3 fromArray4(const std::array<float, 4> a) noexcept
{
	return Vec3{a[0], a[1], a[2], 0.0f};
}

constexpr Vec4 fromArray4asVec4(const std::array<float, 4> a) noexcept
{
	return Vec4{a[0], a[1], a[2], a[3]};
}

constexpr Mat4 fromArray16(const std::array<float, 16> a) noexcept
{
	Mat4 out{};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			out.m(col, row) = a[col * 4 + row];
		}
	}
	return out;
}

} // namespace projectv::math

#endif // PROJECTV_CORE_MATH_HPP
