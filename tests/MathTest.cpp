
#include "core/Math.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace projectv::math::test {

namespace {

constexpr void VerifyLayout()
{
	static_assert(sizeof(Vec3) == 16, "Vec3 must be 16 bytes");
	static_assert(alignof(Vec3) == 16, "Vec3 must be 16-byte aligned");
	static_assert(sizeof(Vec4) == 16, "Vec4 must be 16 bytes");
	static_assert(alignof(Vec4) == 16, "Vec4 must be 16-byte aligned");
	static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes (4 columns x 16 bytes)");
	static_assert(alignof(Mat4) == 16, "Mat4 must be 16-byte aligned");
}

constexpr void VerifyVec3Arithmetic()
{
	const Vec3 a{1.0f, 2.0f, 3.0f, 0.0f};
	const Vec3 b{4.0f, 5.0f, 6.0f, 0.0f};
	if (dot(a, b) != 1.0f * 4.0f + 2.0f * 5.0f + 3.0f * 6.0f) {
		std::fprintf(stderr, "Vec3 dot: %.9f\n", static_cast<double>(dot(a, b)));
		std::abort();
	}
	if (lengthSq(a) != 1.0f + 4.0f + 9.0f) {
		std::fprintf(stderr, "Vec3 lengthSq: %.9f\n", static_cast<double>(lengthSq(a)));
		std::abort();
	}
	if (length(a) != std::sqrt(14.0f)) {
		std::fprintf(stderr, "Vec3 length: %.9f\n", static_cast<double>(length(a)));
		std::abort();
	}
	if (cross(a, b).x != 2.0f * 6.0f - 3.0f * 5.0f
		|| cross(a, b).y != 3.0f * 4.0f - 1.0f * 6.0f
		|| cross(a, b).z != 1.0f * 5.0f - 2.0f * 4.0f) {
		std::fprintf(stderr, "Vec3 cross: %.9f %.9f %.9f\n",
			static_cast<double>(cross(a, b).x),
			static_cast<double>(cross(a, b).y),
			static_cast<double>(cross(a, b).z));
		std::abort();
	}
	const Vec3 n = normalize(a);
	if (std::fabs(length(n) - 1.0f) >= 1e-6f) {
		std::fprintf(stderr, "Vec3 normalize: %.9f\n", static_cast<double>(length(n)));
		std::abort();
	}
	if ((a + b).x != 5.0f || (a - b).y != -3.0f || (-a).z != -3.0f
		|| (a * 2.0f).x != 2.0f || (2.0f * a).y != 4.0f || (b / 2.0f).z != 3.0f) {
		std::fprintf(stderr, "Vec3 +/-/*// failure\n");
		std::abort();
	}
}

constexpr void VerifyVec4Arithmetic()
{
	const Vec4 a{1.0f, 2.0f, 3.0f, 4.0f};
	const Vec4 b{5.0f, 6.0f, 7.0f, 8.0f};
	if (dot(a, b) != 1.0f * 5.0f + 2.0f * 6.0f + 3.0f * 7.0f + 4.0f * 8.0f) {
		std::fprintf(stderr, "Vec4 dot: %.9f\n", static_cast<double>(dot(a, b)));
		std::abort();
	}
	if (lengthSq(a) != 1.0f + 4.0f + 9.0f + 16.0f) {
		std::fprintf(stderr, "Vec4 lengthSq: %.9f\n", static_cast<double>(lengthSq(a)));
		std::abort();
	}
	if (length(a) != std::sqrt(30.0f)) {
		std::fprintf(stderr, "Vec4 length: %.9f\n", static_cast<double>(length(a)));
		std::abort();
	}
	if ((a + b).w != 12.0f || (a - b).w != -4.0f || (-a).x != -1.0f
		|| (a * 3.0f).w != 12.0f) {
		std::fprintf(stderr, "Vec4 +/-/* failure\n");
		std::abort();
	}
}

constexpr void VerifyMat4Identity()
{
	const Mat4 id = identity();
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			const float expected = (col == row) ? 1.0f : 0.0f;
			if (id.m(col, row) != expected) {
				std::fprintf(stderr, "identity(%zu, %zu) = %.9f\n",
					col, row, static_cast<double>(id.m(col, row)));
				std::abort();
			}
		}
	}

	const Vec4 v{1.0f, 2.0f, 3.0f, 4.0f};
	const Vec4 r = id * v;
	if (r.x != 1.0f || r.y != 2.0f || r.z != 3.0f || r.w != 4.0f) {
		std::fprintf(stderr, "identity * vec: %.9f %.9f %.9f %.9f\n",
			static_cast<double>(r.x), static_cast<double>(r.y),
			static_cast<double>(r.z), static_cast<double>(r.w));
		std::abort();
	}
}

constexpr void VerifyMat4Transpose()
{
	Mat4 m{};
	m.c[0] = Vec4{1.0f, 2.0f, 3.0f, 4.0f};
	m.c[1] = Vec4{5.0f, 6.0f, 7.0f, 8.0f};
	m.c[2] = Vec4{9.0f, 10.0f, 11.0f, 12.0f};
	m.c[3] = Vec4{13.0f, 14.0f, 15.0f, 16.0f};

	const Mat4 t = transpose(m);
	const float expected[4][4] = {
		{1.0f, 5.0f, 9.0f, 13.0f},
		{2.0f, 6.0f, 10.0f, 14.0f},
		{3.0f, 7.0f, 11.0f, 15.0f},
		{4.0f, 8.0f, 12.0f, 16.0f},
	};
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			if (t.m(col, row) != expected[col][row]) {
				std::fprintf(stderr, "transpose(%zu, %zu) = %.9f, expected %.9f\n",
					col, row, static_cast<double>(t.m(col, row)),
					static_cast<double>(expected[col][row]));
				std::abort();
			}
		}
	}

	const Mat4 m2 = transpose(t);
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			if (m2.m(col, row) != m.m(col, row)) {
				std::fprintf(stderr, "transpose(transpose) failed at (%zu, %zu)\n", col, row);
				std::abort();
			}
		}
	}
}

constexpr void VerifyMat4Inverse()
{
	const Mat4 invId = inverse(identity());
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			const float expected = (col == row) ? 1.0f : 0.0f;
			if (invId.m(col, row) != expected) {
				std::fprintf(stderr, "inverse(identity)(%zu, %zu) = %.9f\n",
					col, row, static_cast<double>(invId.m(col, row)));
				std::abort();
			}
		}
	}

	Mat4 a{};
	a.c[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
	a.c[1] = Vec4{2.0f, 1.0f, 0.0f, 0.0f};
	a.c[2] = Vec4{3.0f, 4.0f, 1.0f, 0.0f};
	a.c[3] = Vec4{5.0f, 6.0f, 7.0f, 1.0f};
	const Mat4 invA = inverse(a);
	const Mat4 prod = a * invA;
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			const float expected = (col == row) ? 1.0f : 0.0f;
			if (std::fabs(prod.m(col, row) - expected) >= 1e-5f) {
				std::fprintf(stderr, "a * inv(a) at (%zu, %zu) = %.9f, expected %.9f\n",
					col, row, static_cast<double>(prod.m(col, row)),
					static_cast<double>(expected));
				std::abort();
			}
		}
	}
}

constexpr void VerifyMat4Mul()
{
	Mat4 m{};
	m.c[0] = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
	m.c[1] = Vec4{2.0f, 1.0f, 0.0f, 0.0f};
	m.c[2] = Vec4{3.0f, 4.0f, 1.0f, 0.0f};
	m.c[3] = Vec4{5.0f, 6.0f, 7.0f, 1.0f};
	const Mat4 id = identity();
	const Mat4 lhs = m * id;
	const Mat4 rhs = id * m;
	for (std::size_t col = 0; col < 4; ++col) {
		for (std::size_t row = 0; row < 4; ++row) {
			if (lhs.m(col, row) != m.m(col, row) || rhs.m(col, row) != m.m(col, row)) {
				std::fprintf(stderr, "m * identity at (%zu, %zu) failed\n", col, row);
				std::abort();
			}
		}
	}
}

constexpr void VerifyFromArrayHelpers()
{
	const std::array<float, 3> a3{1.0f, 2.0f, 3.0f};
	const Vec3 v3 = fromArray3(a3);
	if (v3.x != 1.0f || v3.y != 2.0f || v3.z != 3.0f) {
		std::fprintf(stderr, "fromArray3 failed\n");
		std::abort();
	}

	const std::array<float, 4> a4{4.0f, 5.0f, 6.0f, 7.0f};
	const Vec3 v3from4 = fromArray4(a4);
	if (v3from4.x != 4.0f || v3from4.y != 5.0f || v3from4.z != 6.0f) {
		std::fprintf(stderr, "fromArray4 (Vec3) failed\n");
		std::abort();
	}
	const Vec4 v4 = fromArray4asVec4(a4);
	if (v4.x != 4.0f || v4.y != 5.0f || v4.z != 6.0f || v4.w != 7.0f) {
		std::fprintf(stderr, "fromArray4asVec4 failed\n");
		std::abort();
	}

	const std::array<float, 16> a16{
		1.0f, 2.0f, 3.0f, 4.0f,
		5.0f, 6.0f, 7.0f, 8.0f,
		9.0f, 10.0f, 11.0f, 12.0f,
		13.0f, 14.0f, 15.0f, 16.0f,
	};
	const Mat4 m16 = fromArray16(a16);
	if (m16.m(0, 0) != 1.0f || m16.m(1, 1) != 6.0f
		|| m16.m(2, 2) != 11.0f || m16.m(3, 3) != 16.0f) {
		std::fprintf(stderr, "fromArray16 failed\n");
		std::abort();
	}
}

} // namespace
} // namespace projectv::math::test

int main()
{
	projectv::math::test::VerifyLayout();
	projectv::math::test::VerifyVec3Arithmetic();
	projectv::math::test::VerifyVec4Arithmetic();
	projectv::math::test::VerifyMat4Identity();
	projectv::math::test::VerifyMat4Transpose();
	projectv::math::test::VerifyMat4Inverse();
	projectv::math::test::VerifyMat4Mul();
	projectv::math::test::VerifyFromArrayHelpers();
	std::printf("MathTest: 8/8 passed\n");
	return 0;
}
