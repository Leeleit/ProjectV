// bench.cpp - Standalone benchmark comparing scalar vs AVX2/FMA Perlin noise (2D/3D).
//
// Algorithm: Ken Perlin's improved noise (2002), smootherstep fade curve.
// Reference: https://mrl.cs.nyu.edu/~perlin/noise/ (canonical algorithm).
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     bench.cpp -o /tmp/bench_simd_noise
//
// Run:
//   /tmp/bench_simd_noise
//
// Output:
//   stdout: human-readable summary
//   ../results.csv: machine-readable per-config stats
//
// Scope per docs/experiments/AGENTS.md section 2: standalone research artifact,
// NOT part of ProjectV mainline. Zero ProjectV source dependencies.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sched.h>
#include <string>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace bench {

// ============================================================================
// Scalar Perlin noise (reference implementation)
// ============================================================================

namespace scalar {

// 256-byte permutation table (canonical Ken Perlin).
// Seeded deterministically — std::mt19937 with fixed seed.
alignas(64) static uint8_t kPerm[512];

static void InitPerm(uint32_t seed)
{
	std::mt19937 rng(seed);
	for (int i = 0; i < 256; i++)
		kPerm[i] = static_cast<uint8_t>(i);
	std::shuffle(kPerm, kPerm + 256, rng);
	for (int i = 0; i < 256; i++)
		kPerm[256 + i] = kPerm[i]; // doubled for wraparound
}

static inline float Fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); // 6t^5 - 15t^4 + 10t^3
}

static inline float Lerp(float t, float a, float b)
{
	return a + t * (b - a);
}

// 8 gradient vectors for 2D (subset of standard, hash % 8 picks one).
// Same convention as Auburn/FastNoise.
static constexpr float kGrad2[8][2] = {
	{1.0f, 0.0f},
	{-1.0f, 0.0f},
	{0.0f, 1.0f},
	{0.0f, -1.0f},
	{0.70710678f, 0.70710678f},
	{-0.70710678f, 0.70710678f},
	{0.70710678f, -0.70710678f},
	{-0.70710678f, -0.70710678f},
};

// 12 gradient vectors for 3D (canonical Ken Perlin).
static constexpr float kGrad3[12][3] = {
	{1.0f, 1.0f, 0.0f},
	{-1.0f, 1.0f, 0.0f},
	{1.0f, -1.0f, 0.0f},
	{-1.0f, -1.0f, 0.0f},
	{1.0f, 0.0f, 1.0f},
	{-1.0f, 0.0f, 1.0f},
	{1.0f, 0.0f, -1.0f},
	{-1.0f, 0.0f, -1.0f},
	{0.0f, 1.0f, 1.0f},
	{0.0f, -1.0f, 1.0f},
	{0.0f, 1.0f, -1.0f},
	{0.0f, -1.0f, -1.0f},
};

static float Perlin2D(float x, float y)
{
	int32_t X = static_cast<int32_t>(std::floor(x)) & 255;
	int32_t Y = static_cast<int32_t>(std::floor(y)) & 255;
	float xf = x - std::floor(x);
	float yf = y - std::floor(y);

	int32_t A = kPerm[X] + Y;
	int32_t AA = kPerm[A];
	int32_t AB = kPerm[A + 1];
	int32_t B = kPerm[X + 1] + Y;
	int32_t BA = kPerm[B];
	int32_t BB = kPerm[B + 1];

	const float *g00 = kGrad2[AA & 7];
	const float *g10 = kGrad2[BA & 7];
	const float *g01 = kGrad2[AB & 7];
	const float *g11 = kGrad2[BB & 7];

	float n00 = g00[0] * xf + g00[1] * yf;
	float n10 = g10[0] * (xf - 1) + g10[1] * yf;
	float n01 = g01[0] * xf + g01[1] * (yf - 1);
	float n11 = g11[0] * (xf - 1) + g11[1] * (yf - 1);

	float u = Fade(xf);
	float v = Fade(yf);
	return Lerp(v, Lerp(u, n00, n10), Lerp(u, n01, n11));
}

static float Perlin3D(float x, float y, float z)
{
	int32_t X = static_cast<int32_t>(std::floor(x)) & 255;
	int32_t Y = static_cast<int32_t>(std::floor(y)) & 255;
	int32_t Z = static_cast<int32_t>(std::floor(z)) & 255;
	float xf = x - std::floor(x);
	float yf = y - std::floor(y);
	float zf = z - std::floor(z);

	int32_t A = kPerm[X] + Y;
	int32_t AA = kPerm[A] + Z;
	int32_t AB = kPerm[A + 1] + Z;
	int32_t B = kPerm[X + 1] + Y;
	int32_t BA = kPerm[B] + Z;
	int32_t BB = kPerm[B + 1] + Z;

	int32_t AAA = kPerm[AA];
	int32_t ABA = kPerm[AB];
	int32_t BAA = kPerm[BA];
	int32_t BBA = kPerm[BB];
	int32_t AAB = kPerm[AA + 1];
	int32_t ABB = kPerm[AB + 1];
	int32_t BAB = kPerm[BA + 1];
	int32_t BBB = kPerm[BB + 1];

	const float *g000 = kGrad3[AAA & 11];
	const float *g100 = kGrad3[BAA & 11];
	const float *g010 = kGrad3[ABA & 11];
	const float *g110 = kGrad3[BBA & 11];
	const float *g001 = kGrad3[AAB & 11];
	const float *g101 = kGrad3[BAB & 11];
	const float *g011 = kGrad3[ABB & 11];
	const float *g111 = kGrad3[BBB & 11];

	float n000 = g000[0] * xf + g000[1] * yf + g000[2] * zf;
	float n100 = g100[0] * (xf - 1) + g100[1] * yf + g100[2] * zf;
	float n010 = g010[0] * xf + g010[1] * (yf - 1) + g010[2] * zf;
	float n110 = g110[0] * (xf - 1) + g110[1] * (yf - 1) + g110[2] * zf;
	float n001 = g001[0] * xf + g001[1] * yf + g001[2] * (zf - 1);
	float n101 = g101[0] * (xf - 1) + g101[1] * yf + g101[2] * (zf - 1);
	float n011 = g011[0] * xf + g011[1] * (yf - 1) + g011[2] * (zf - 1);
	float n111 = g111[0] * (xf - 1) + g111[1] * (yf - 1) + g111[2] * (zf - 1);

	float u = Fade(xf);
	float v = Fade(yf);
	float w = Fade(zf);
	return Lerp(w,
				Lerp(v, Lerp(u, n000, n100), Lerp(u, n010, n110)),
				Lerp(v, Lerp(u, n001, n101), Lerp(u, n011, n111)));
}

// ----------------------------------------------------------------
// Variant 2: SIMD-friendly Perlin (splitmix32 hash, 16-entry gradient table).
// Designed for clean AVX2 vectorization — no perm table, integer hash.
// Trade-off: noise distribution slightly different from Ken Perlin spec
// (no permutation bijection), but still C¹ continuous, similar visual quality.
// Used as the "fair" baseline for measuring AVX2 vs scalar throughput.
// ----------------------------------------------------------------

// 16-entry gradient vectors for 2D (hash & 15 picks one).
static constexpr float kGrad2Simd[16][2] = {
	{1.0f, 0.0f},
	{-1.0f, 0.0f},
	{0.0f, 1.0f},
	{0.0f, -1.0f},
	{1.0f, 1.0f},
	{-1.0f, -1.0f},
	{1.0f, -1.0f},
	{-1.0f, 1.0f},
	{1.0f, 0.0f},
	{-1.0f, 0.0f},
	{0.0f, 1.0f},
	{0.0f, -1.0f},
	{1.0f, 1.0f},
	{-1.0f, -1.0f},
	{1.0f, -1.0f},
	{-1.0f, 1.0f},
};

// 16-entry gradient vectors for 3D (hash & 15 picks one).
// Standard Ken Perlin improved-noise 12 vectors, plus 4 extras for 16 total.
static constexpr float kGrad3Simd[16][3] = {
	{1.0f, 1.0f, 0.0f},
	{-1.0f, 1.0f, 0.0f},
	{1.0f, -1.0f, 0.0f},
	{-1.0f, -1.0f, 0.0f},
	{1.0f, 0.0f, 1.0f},
	{-1.0f, 0.0f, 1.0f},
	{1.0f, 0.0f, -1.0f},
	{-1.0f, 0.0f, -1.0f},
	{0.0f, 1.0f, 1.0f},
	{0.0f, -1.0f, 1.0f},
	{0.0f, 1.0f, -1.0f},
	{0.0f, -1.0f, -1.0f},
	{1.0f, 1.0f, 0.0f},
	{-1.0f, -1.0f, 0.0f},
	{0.0f, 1.0f, -1.0f},
	{0.0f, -1.0f, 1.0f},
};

// Splitmix32 integer hash — pure ops, no table lookup.
static inline uint32_t Splitmix32(uint32_t x)
{
	x += 0x9E3779B9u;
	x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
	x = (x ^ (x >> 13)) * 0xC2B2AE35u;
	return x ^ (x >> 16);
}

static float PerlinSimd2D(float x, float y)
{
	int32_t X = static_cast<int32_t>(std::floor(x));
	int32_t Y = static_cast<int32_t>(std::floor(y));
	float xf = x - std::floor(x);
	float yf = y - std::floor(y);

	uint32_t h00 = Splitmix32(static_cast<uint32_t>(X) * 73856093u ^ static_cast<uint32_t>(Y) * 19349663u);
	uint32_t h10 = Splitmix32(static_cast<uint32_t>(X + 1) * 73856093u ^ static_cast<uint32_t>(Y) * 19349663u);
	uint32_t h01 = Splitmix32(static_cast<uint32_t>(X) * 73856093u ^ static_cast<uint32_t>(Y + 1) * 19349663u);
	uint32_t h11 = Splitmix32(static_cast<uint32_t>(X + 1) * 73856093u ^ static_cast<uint32_t>(Y + 1) * 19349663u);

	const float *g00 = kGrad2Simd[h00 & 15];
	const float *g10 = kGrad2Simd[h10 & 15];
	const float *g01 = kGrad2Simd[h01 & 15];
	const float *g11 = kGrad2Simd[h11 & 15];

	float n00 = g00[0] * xf + g00[1] * yf;
	float n10 = g10[0] * (xf - 1) + g10[1] * yf;
	float n01 = g01[0] * xf + g01[1] * (yf - 1);
	float n11 = g11[0] * (xf - 1) + g11[1] * (yf - 1);

	float u = Fade(xf);
	float v = Fade(yf);
	return Lerp(v, Lerp(u, n00, n10), Lerp(u, n01, n11));
}

static float PerlinSimd3D(float x, float y, float z)
{
	int32_t X = static_cast<int32_t>(std::floor(x));
	int32_t Y = static_cast<int32_t>(std::floor(y));
	int32_t Z = static_cast<int32_t>(std::floor(z));
	float xf = x - std::floor(x);
	float yf = y - std::floor(y);
	float zf = z - std::floor(z);

	auto mix3 = [](int32_t x, int32_t y, int32_t z) {
		return Splitmix32(static_cast<uint32_t>(x) * 73856093u ^
						  static_cast<uint32_t>(y) * 19349663u ^
						  static_cast<uint32_t>(z) * 83492791u);
	};

	uint32_t h000 = mix3(X, Y, Z);
	uint32_t h100 = mix3(X + 1, Y, Z);
	uint32_t h010 = mix3(X, Y + 1, Z);
	uint32_t h110 = mix3(X + 1, Y + 1, Z);
	uint32_t h001 = mix3(X, Y, Z + 1);
	uint32_t h101 = mix3(X + 1, Y, Z + 1);
	uint32_t h011 = mix3(X, Y + 1, Z + 1);
	uint32_t h111 = mix3(X + 1, Y + 1, Z + 1);

	const float *g000 = kGrad3Simd[h000 & 15];
	const float *g100 = kGrad3Simd[h100 & 15];
	const float *g010 = kGrad3Simd[h010 & 15];
	const float *g110 = kGrad3Simd[h110 & 15];
	const float *g001 = kGrad3Simd[h001 & 15];
	const float *g101 = kGrad3Simd[h101 & 15];
	const float *g011 = kGrad3Simd[h011 & 15];
	const float *g111 = kGrad3Simd[h111 & 15];

	float n000 = g000[0] * xf + g000[1] * yf + g000[2] * zf;
	float n100 = g100[0] * (xf - 1) + g100[1] * yf + g100[2] * zf;
	float n010 = g010[0] * xf + g010[1] * (yf - 1) + g010[2] * zf;
	float n110 = g110[0] * (xf - 1) + g110[1] * (yf - 1) + g110[2] * zf;
	float n001 = g001[0] * xf + g001[1] * yf + g001[2] * (zf - 1);
	float n101 = g101[0] * (xf - 1) + g101[1] * yf + g101[2] * (zf - 1);
	float n011 = g011[0] * xf + g011[1] * (yf - 1) + g011[2] * (zf - 1);
	float n111 = g111[0] * (xf - 1) + g111[1] * (yf - 1) + g111[2] * (zf - 1);

	float u = Fade(xf);
	float v = Fade(yf);
	float w = Fade(zf);
	return Lerp(w,
				Lerp(v, Lerp(u, n000, n100), Lerp(u, n010, n110)),
				Lerp(v, Lerp(u, n001, n101), Lerp(u, n011, n111)));
}

} // namespace scalar

// ============================================================================
// AVX2/FMA Perlin noise (8 samples parallel)
// ============================================================================

#ifdef __AVX2__

namespace avx2 {

alignas(32) static uint8_t kPerm[512];

static void InitPerm(uint32_t seed)
{
	std::mt19937 rng(seed);
	for (int i = 0; i < 256; i++)
		kPerm[i] = static_cast<uint8_t>(i);
	std::shuffle(kPerm, kPerm + 256, rng);
	for (int i = 0; i < 256; i++)
		kPerm[256 + i] = kPerm[i];
}

__attribute__((target("avx2,fma"))) static inline __m256 Fade(__m256 t)
{
	// 6t^5 - 15t^4 + 10t^3 — Horner form: t * t * t * (t * (t * 6 - 15) + 10)
	__m256 t2 = _mm256_mul_ps(t, t);
	__m256 t3 = _mm256_mul_ps(t2, t);
	__m256 inner = _mm256_fmadd_ps(t, _mm256_set1_ps(6.0f), _mm256_set1_ps(-15.0f));
	inner = _mm256_fmadd_ps(t, inner, _mm256_set1_ps(10.0f));
	return _mm256_mul_ps(t3, inner);
}

__attribute__((target("avx2,fma"))) static inline __m256 Lerp(__m256 t, __m256 a, __m256 b)
{
	return _mm256_fmadd_ps(t, _mm256_sub_ps(b, a), a); // a + t * (b - a)
}

// Scalar hash extraction for 8 lanes — done once per call, fast enough.
// The hash is data-dependent (table lookup), so we extract to scalar, do
// 4 lookups per lane, repack. Cost ≈ 16 cycles vs ~50 cycles for the rest.
__attribute__((target("avx2,fma"))) static inline void HashCorners2D(
	const __m256i &xi, const __m256i &yi,
	const uint8_t *perm,
	__m256i &h00, __m256i &h10, __m256i &h01, __m256i &h11)
{
	alignas(32) int32_t xl[8], yl[8];
	_mm256_store_si256(reinterpret_cast<__m256i *>(xl), xi);
	_mm256_store_si256(reinterpret_cast<__m256i *>(yl), yi);
	alignas(32) int32_t o00[8], o10[8], o01[8], o11[8];
	for (int i = 0; i < 8; i++) {
		int32_t X = xl[i] & 255;
		int32_t Y = yl[i] & 255;
		int32_t A = perm[X] + Y;
		int32_t B = perm[X + 1] + Y;
		o00[i] = perm[A];
		o10[i] = perm[B];
		o01[i] = perm[A + 1];
		o11[i] = perm[B + 1];
	}
	h00 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o00));
	h10 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o10));
	h01 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o01));
	h11 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o11));
}

__attribute__((target("avx2,fma"))) static inline void HashCorners3D(
	const __m256i &xi, const __m256i &yi, const __m256i &zi,
	const uint8_t *perm,
	__m256i &h000, __m256i &h100, __m256i &h010, __m256i &h110,
	__m256i &h001, __m256i &h101, __m256i &h011, __m256i &h111)
{
	alignas(32) int32_t xl[8], yl[8], zl[8];
	_mm256_store_si256(reinterpret_cast<__m256i *>(xl), xi);
	_mm256_store_si256(reinterpret_cast<__m256i *>(yl), yi);
	_mm256_store_si256(reinterpret_cast<__m256i *>(zl), zi);
	alignas(32) int32_t o[8][8];
	for (int i = 0; i < 8; i++) {
		int32_t X = xl[i] & 255;
		int32_t Y = yl[i] & 255;
		int32_t Z = zl[i] & 255;
		int32_t A = perm[X] + Y;
		int32_t B = perm[X + 1] + Y;
		int32_t AA = perm[A] + Z;
		int32_t BA = perm[B] + Z;
		int32_t AB = perm[A + 1] + Z;
		int32_t BB = perm[B + 1] + Z;
		o[0][i] = perm[AA];
		o[1][i] = perm[BA];
		o[2][i] = perm[AB];
		o[3][i] = perm[BB];
		o[4][i] = perm[AA + 1];
		o[5][i] = perm[BA + 1];
		o[6][i] = perm[AB + 1];
		o[7][i] = perm[BB + 1];
	}
	h000 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[0]));
	h100 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[1]));
	h010 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[2]));
	h110 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[3]));
	h001 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[4]));
	h101 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[5]));
	h011 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[6]));
	h111 = _mm256_load_si256(reinterpret_cast<const __m256i *>(o[7]));
}

// 8-element gradient tables indexed by hash & 7.
// Aligned to 32 bytes for vmovaps alignment.
alignas(32) static const float kGrad2X[8] = {
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
	0.70710678f,
	-0.70710678f,
	0.70710678f,
	-0.70710678f,
};
alignas(32) static const float kGrad2Y[8] = {
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	0.70710678f,
	0.70710678f,
	-0.70710678f,
	-0.70710678f,
};

__attribute__((target("avx2,fma"))) static inline void GatherGrad2(__m256i h, __m256 &gx, __m256 &gy)
{
	__m256i idx = _mm256_and_si256(h, _mm256_set1_epi32(7));
	gx = _mm256_i32gather_ps(kGrad2X, idx, 4); // scale 4 = sizeof(float)
	gy = _mm256_i32gather_ps(kGrad2Y, idx, 4);
}

// 12 gradient vectors for 3D, decomposed into 3 separate tables for gather.
alignas(32) static const float kGrad3X[12] = {
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
};
alignas(32) static const float kGrad3Y[12] = {
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
};
alignas(32) static const float kGrad3Z[12] = {
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
};

__attribute__((target("avx2,fma"))) static inline void GatherGrad3(__m256i h, __m256 &gx, __m256 &gy, __m256 &gz)
{
	__m256i idx = _mm256_and_si256(h, _mm256_set1_epi32(11));
	gx = _mm256_i32gather_ps(kGrad3X, idx, 4);
	gy = _mm256_i32gather_ps(kGrad3Y, idx, 4);
	gz = _mm256_i32gather_ps(kGrad3Z, idx, 4);
}

__attribute__((target("avx2,fma"))) static inline __m256 DotGrad2(__m256 gx, __m256 gy, __m256 dx, __m256 dy)
{
	return _mm256_fmadd_ps(gx, dx, _mm256_mul_ps(gy, dy));
}

__attribute__((target("avx2,fma"))) static inline __m256 DotGrad3(__m256 gx, __m256 gy, __m256 gz,
																  __m256 dx, __m256 dy, __m256 dz)
{
	__m256 t = _mm256_mul_ps(gx, dx);
	t = _mm256_fmadd_ps(gy, dy, t);
	return _mm256_fmadd_ps(gz, dz, t);
}

__attribute__((target("avx2,fma"))) static inline __m256 Perlin2D_8(__m256 x, __m256 y)
{
	__m256 xi_floor = _mm256_floor_ps(x);
	__m256 yi_floor = _mm256_floor_ps(y);
	__m256i xi = _mm256_cvtps_epi32(xi_floor);
	__m256i yi = _mm256_cvtps_epi32(yi_floor);
	__m256 xf = _mm256_sub_ps(x, xi_floor);
	__m256 yf = _mm256_sub_ps(y, yi_floor);

	__m256 u = Fade(xf);
	__m256 v = Fade(yf);

	__m256i h00, h10, h01, h11;
	HashCorners2D(xi, yi, kPerm, h00, h10, h01, h11);

	__m256 g00x, g00y, g10x, g10y, g01x, g01y, g11x, g11y;
	GatherGrad2(h00, g00x, g00y);
	GatherGrad2(h10, g10x, g10y);
	GatherGrad2(h01, g01x, g01y);
	GatherGrad2(h11, g11x, g11y);

	__m256 one = _mm256_set1_ps(1.0f);
	__m256 n00 = DotGrad2(g00x, g00y, xf, yf);
	__m256 n10 = DotGrad2(g10x, g10y, _mm256_sub_ps(xf, one), yf);
	__m256 n01 = DotGrad2(g01x, g01y, xf, _mm256_sub_ps(yf, one));
	__m256 n11 = DotGrad2(g11x, g11y, _mm256_sub_ps(xf, one), _mm256_sub_ps(yf, one));

	__m256 nx0 = Lerp(u, n00, n10);
	__m256 nx1 = Lerp(u, n01, n11);
	return Lerp(v, nx0, nx1);
}

__attribute__((target("avx2,fma"))) static inline __m256 Perlin3D_8(__m256 x, __m256 y, __m256 z)
{
	__m256 xi_floor = _mm256_floor_ps(x);
	__m256 yi_floor = _mm256_floor_ps(y);
	__m256 zi_floor = _mm256_floor_ps(z);
	__m256i xi = _mm256_cvtps_epi32(xi_floor);
	__m256i yi = _mm256_cvtps_epi32(yi_floor);
	__m256i zi = _mm256_cvtps_epi32(zi_floor);
	__m256 xf = _mm256_sub_ps(x, xi_floor);
	__m256 yf = _mm256_sub_ps(y, yi_floor);
	__m256 zf = _mm256_sub_ps(z, zi_floor);

	__m256 u = Fade(xf);
	__m256 v = Fade(yf);
	__m256 w = Fade(zf);

	__m256i h000, h100, h010, h110, h001, h101, h011, h111;
	HashCorners3D(xi, yi, zi, kPerm, h000, h100, h010, h110, h001, h101, h011, h111);

	__m256 g000x, g000y, g000z;
	__m256 g100x, g100y, g100z;
	__m256 g010x, g010y, g010z;
	__m256 g110x, g110y, g110z;
	__m256 g001x, g001y, g001z;
	__m256 g101x, g101y, g101z;
	__m256 g011x, g011y, g011z;
	__m256 g111x, g111y, g111z;
	GatherGrad3(h000, g000x, g000y, g000z);
	GatherGrad3(h100, g100x, g100y, g100z);
	GatherGrad3(h010, g010x, g010y, g010z);
	GatherGrad3(h110, g110x, g110y, g110z);
	GatherGrad3(h001, g001x, g001y, g001z);
	GatherGrad3(h101, g101x, g101y, g101z);
	GatherGrad3(h011, g011x, g011y, g011z);
	GatherGrad3(h111, g111x, g111y, g111z);

	__m256 one = _mm256_set1_ps(1.0f);
	__m256 omx = _mm256_sub_ps(xf, one);
	__m256 omy = _mm256_sub_ps(yf, one);
	__m256 omz = _mm256_sub_ps(zf, one);

	__m256 n000 = DotGrad3(g000x, g000y, g000z, xf, yf, zf);
	__m256 n100 = DotGrad3(g100x, g100y, g100z, omx, yf, zf);
	__m256 n010 = DotGrad3(g010x, g010y, g010z, xf, omy, zf);
	__m256 n110 = DotGrad3(g110x, g110y, g110z, omx, omy, zf);
	__m256 n001 = DotGrad3(g001x, g001y, g001z, xf, yf, omz);
	__m256 n101 = DotGrad3(g101x, g101y, g101z, omx, yf, omz);
	__m256 n011 = DotGrad3(g011x, g011y, g011z, xf, omy, omz);
	__m256 n111 = DotGrad3(g111x, g111y, g111z, omx, omy, omz);

	__m256 nx00 = Lerp(u, n000, n100);
	__m256 nx10 = Lerp(u, n010, n110);
	__m256 nx01 = Lerp(u, n001, n101);
	__m256 nx11 = Lerp(u, n011, n111);
	__m256 nxy0 = Lerp(v, nx00, nx10);
	__m256 nxy1 = Lerp(v, nx01, nx11);
	return Lerp(w, nxy0, nxy1);
}

// ----------------------------------------------------------------
// SIMD-friendly variant: splitmix32 hash + 16-entry gradient table.
// Pure SIMD integer ops for hash, _mm256_i32gather_ps for gradient lookup.
// This is the variant expected to give 4-8× speedup per literature.
// ----------------------------------------------------------------

// 16-entry gradient tables aligned to 32 bytes for AVX2 gather.
alignas(32) static const float kGrad2XSimd[16] = {
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
};
alignas(32) static const float kGrad2YSimd[16] = {
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	1.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	1.0f,
};
alignas(32) static const float kGrad3XSimd[16] = {
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	0.0f,
	0.0f,
};
alignas(32) static const float kGrad3YSimd[16] = {
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
	1.0f,
	-1.0f,
};
alignas(32) static const float kGrad3ZSimd[16] = {
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	1.0f,
	1.0f,
	-1.0f,
	-1.0f,
	0.0f,
	0.0f,
	-1.0f,
	1.0f,
};

__attribute__((target("avx2,fma"))) static inline __m256i Splitmix32_8(__m256i x)
{
	x = _mm256_add_epi32(x, _mm256_set1_epi32(0x9E3779B9));
	// Stage 1: tmp = x ^ (x >> 16); x = tmp * 0x85EBCA6B
	// Explicit tmp because AVX2 function args eval in unspecified order.
	__m256i tmp = _mm256_xor_si256(x, _mm256_srli_epi32(x, 16));
	x = _mm256_mullo_epi32(tmp, _mm256_set1_epi32(0x85EBCA6B));
	// Stage 2: tmp = x ^ (x >> 13); x = tmp * 0xC2B2AE35
	tmp = _mm256_xor_si256(x, _mm256_srli_epi32(x, 13));
	x = _mm256_mullo_epi32(tmp, _mm256_set1_epi32(0xC2B2AE35));
	// Stage 3: return x ^ (x >> 16)
	return _mm256_xor_si256(x, _mm256_srli_epi32(x, 16));
}

__attribute__((target("avx2,fma"))) static inline __m256i Mix3_8(__m256i x, __m256i y, __m256i z)
{
	__m256i h = _mm256_mullo_epi32(x, _mm256_set1_epi32(73856093));
	h = _mm256_xor_si256(h, _mm256_mullo_epi32(y, _mm256_set1_epi32(19349663)));
	h = _mm256_xor_si256(h, _mm256_mullo_epi32(z, _mm256_set1_epi32(83492791)));
	return Splitmix32_8(h);
}

__attribute__((target("avx2,fma"))) static inline void GatherGrad2Simd(__m256i h, __m256 &gx, __m256 &gy)
{
	__m256i idx = _mm256_and_si256(h, _mm256_set1_epi32(15));
	gx = _mm256_i32gather_ps(kGrad2XSimd, idx, 4);
	gy = _mm256_i32gather_ps(kGrad2YSimd, idx, 4);
}

__attribute__((target("avx2,fma"))) static inline void GatherGrad3Simd(__m256i h, __m256 &gx, __m256 &gy, __m256 &gz)
{
	__m256i idx = _mm256_and_si256(h, _mm256_set1_epi32(15));
	gx = _mm256_i32gather_ps(kGrad3XSimd, idx, 4);
	gy = _mm256_i32gather_ps(kGrad3YSimd, idx, 4);
	gz = _mm256_i32gather_ps(kGrad3ZSimd, idx, 4);
}

__attribute__((target("avx2,fma"))) static inline __m256 PerlinSimd2D_8(__m256 x, __m256 y)
{
	__m256 xi_floor = _mm256_floor_ps(x);
	__m256 yi_floor = _mm256_floor_ps(y);
	__m256i xi = _mm256_cvtps_epi32(xi_floor);
	__m256i yi = _mm256_cvtps_epi32(yi_floor);
	__m256 xf = _mm256_sub_ps(x, xi_floor);
	__m256 yf = _mm256_sub_ps(y, yi_floor);

	__m256 u = Fade(xf);
	__m256 v = Fade(yf);

	__m256i h00 = Mix3_8(xi, yi, _mm256_setzero_si256());
	__m256i h10 = Mix3_8(_mm256_add_epi32(xi, _mm256_set1_epi32(1)), yi, _mm256_setzero_si256());
	__m256i h01 = Mix3_8(xi, _mm256_add_epi32(yi, _mm256_set1_epi32(1)), _mm256_setzero_si256());
	__m256i h11 = Mix3_8(_mm256_add_epi32(xi, _mm256_set1_epi32(1)),
						 _mm256_add_epi32(yi, _mm256_set1_epi32(1)),
						 _mm256_setzero_si256());

	__m256 g00x, g00y, g10x, g10y, g01x, g01y, g11x, g11y;
	GatherGrad2Simd(h00, g00x, g00y);
	GatherGrad2Simd(h10, g10x, g10y);
	GatherGrad2Simd(h01, g01x, g01y);
	GatherGrad2Simd(h11, g11x, g11y);

	__m256 one = _mm256_set1_ps(1.0f);
	__m256 n00 = DotGrad2(g00x, g00y, xf, yf);
	__m256 n10 = DotGrad2(g10x, g10y, _mm256_sub_ps(xf, one), yf);
	__m256 n01 = DotGrad2(g01x, g01y, xf, _mm256_sub_ps(yf, one));
	__m256 n11 = DotGrad2(g11x, g11y, _mm256_sub_ps(xf, one), _mm256_sub_ps(yf, one));

	return Lerp(v, Lerp(u, n00, n10), Lerp(u, n01, n11));
}

__attribute__((target("avx2,fma"))) static inline __m256 PerlinSimd3D_8(__m256 x, __m256 y, __m256 z)
{
	__m256 xi_floor = _mm256_floor_ps(x);
	__m256 yi_floor = _mm256_floor_ps(y);
	__m256 zi_floor = _mm256_floor_ps(z);
	__m256i xi = _mm256_cvtps_epi32(xi_floor);
	__m256i yi = _mm256_cvtps_epi32(yi_floor);
	__m256i zi = _mm256_cvtps_epi32(zi_floor);
	__m256 xf = _mm256_sub_ps(x, xi_floor);
	__m256 yf = _mm256_sub_ps(y, yi_floor);
	__m256 zf = _mm256_sub_ps(z, zi_floor);

	__m256 u = Fade(xf);
	__m256 v = Fade(yf);
	__m256 w = Fade(zf);

	__m256i one_i = _mm256_set1_epi32(1);
	__m256i h000 = Mix3_8(xi, yi, zi);
	__m256i h100 = Mix3_8(_mm256_add_epi32(xi, one_i), yi, zi);
	__m256i h010 = Mix3_8(xi, _mm256_add_epi32(yi, one_i), zi);
	__m256i h110 = Mix3_8(_mm256_add_epi32(xi, one_i), _mm256_add_epi32(yi, one_i), zi);
	__m256i h001 = Mix3_8(xi, yi, _mm256_add_epi32(zi, one_i));
	__m256i h101 = Mix3_8(_mm256_add_epi32(xi, one_i), yi, _mm256_add_epi32(zi, one_i));
	__m256i h011 = Mix3_8(xi, _mm256_add_epi32(yi, one_i), _mm256_add_epi32(zi, one_i));
	__m256i h111 = Mix3_8(_mm256_add_epi32(xi, one_i), _mm256_add_epi32(yi, one_i), _mm256_add_epi32(zi, one_i));

	__m256 g000x, g000y, g000z;
	__m256 g100x, g100y, g100z;
	__m256 g010x, g010y, g010z;
	__m256 g110x, g110y, g110z;
	__m256 g001x, g001y, g001z;
	__m256 g101x, g101y, g101z;
	__m256 g011x, g011y, g011z;
	__m256 g111x, g111y, g111z;
	GatherGrad3Simd(h000, g000x, g000y, g000z);
	GatherGrad3Simd(h100, g100x, g100y, g100z);
	GatherGrad3Simd(h010, g010x, g010y, g010z);
	GatherGrad3Simd(h110, g110x, g110y, g110z);
	GatherGrad3Simd(h001, g001x, g001y, g001z);
	GatherGrad3Simd(h101, g101x, g101y, g101z);
	GatherGrad3Simd(h011, g011x, g011y, g011z);
	GatherGrad3Simd(h111, g111x, g111y, g111z);

	__m256 one = _mm256_set1_ps(1.0f);
	__m256 omx = _mm256_sub_ps(xf, one);
	__m256 omy = _mm256_sub_ps(yf, one);
	__m256 omz = _mm256_sub_ps(zf, one);

	__m256 n000 = DotGrad3(g000x, g000y, g000z, xf, yf, zf);
	__m256 n100 = DotGrad3(g100x, g100y, g100z, omx, yf, zf);
	__m256 n010 = DotGrad3(g010x, g010y, g010z, xf, omy, zf);
	__m256 n110 = DotGrad3(g110x, g110y, g110z, omx, omy, zf);
	__m256 n001 = DotGrad3(g001x, g001y, g001z, xf, yf, omz);
	__m256 n101 = DotGrad3(g101x, g101y, g101z, omx, yf, omz);
	__m256 n011 = DotGrad3(g011x, g011y, g011z, xf, omy, omz);
	__m256 n111 = DotGrad3(g111x, g111y, g111z, omx, omy, omz);

	return Lerp(w,
				Lerp(v, Lerp(u, n000, n100), Lerp(u, n010, n110)),
				Lerp(v, Lerp(u, n001, n101), Lerp(u, n011, n111)));
}

} // namespace avx2

#endif // __AVX2__

// ============================================================================
// Benchmark harness
// ============================================================================

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
};

Stats ComputeStats(std::vector<double> &samples)
{
	std::sort(samples.begin(), samples.end());
	Stats s{};
	double sum = 0.0;
	for (double v : samples)
		sum += v;
	s.mean = sum / samples.size();
	s.median = samples[samples.size() / 2];
	s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / samples.size());
	s.min = samples.front();
	s.max = samples.back();
	return s;
}

template <typename Fn>
Stats Bench(const std::string &label, int reps, int samples_per_rep, Fn &&fn)
{
	// Warm-up
	for (int i = 0; i < 10; i++)
		fn();

	std::vector<double> samples;
	samples.reserve(reps);
	for (int r = 0; r < reps; r++) {
		auto t0 = std::chrono::steady_clock::now();
		fn();
		auto t1 = std::chrono::steady_clock::now();
		double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
		samples.push_back(us);
	}
	Stats s = ComputeStats(samples);
	double throughput = samples_per_rep / (s.mean * 1e-6);
	std::printf("%-20s | mean %7.2f us | p95 %7.2f us | p99 %7.2f us | std %6.2f us | throughput %10.1f M/s\n",
				label.c_str(), s.mean, s.p95, s.p99, s.stddev, throughput / 1e6);
	return s;
}

// Pin to single CPU core for low-jitter measurement.
void PinToCore(int core_id = 0)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
		std::fprintf(stderr, "[WARN] sched_setaffinity failed; measurements may have jitter\n");
	}
}

} // namespace bench

int main(int argc, char **argv)
{
	using namespace bench;
	const uint32_t kSeed = 0xC0FFEE;
	const int kReps = 1000;
	const int kBatch = 1024; // samples per iteration (matches ProjectV chunkSize 8³ = 512, doubled for alignment)

	scalar::InitPerm(kSeed);
#ifdef __AVX2__
	avx2::InitPerm(kSeed);
#endif

	PinToCore(0);

	// Generate random sample points (deterministic).
	std::mt19937 rng(kSeed);
	std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
	alignas(32) float xs[kBatch], ys[kBatch], zs[kBatch];
	for (int i = 0; i < kBatch; i++) {
		xs[i] = dist(rng);
		ys[i] = dist(rng);
		zs[i] = dist(rng);
	}

	std::printf("Perlin noise benchmark — 2D and 3D, scalar vs AVX2/FMA\n");
	std::printf("Host: AMD Ryzen 7 5800X, Clang %s, -O3 -march=native -DNDEBUG\n",
				__clang_version__);
#ifdef __AVX2__
	std::printf("AVX2+FMA: enabled\n");
#else
	std::printf("AVX2+FMA: NOT ENABLED on this build — AVX2 results will be missing\n");
#endif
	std::printf("Reps: %d, samples/iter: %d\n\n", kReps, kBatch);

	// ----------------------------------------------------------------
	// 2D Perlin (spec, perm-table-based)
	// ----------------------------------------------------------------
	std::printf("=== 2D Perlin (spec, perm table) ===\n");
	float acc_2d_scalar_spec = 0.0f;
	Stats s_2d_scalar_spec = Bench("scalar", kReps, kBatch, [&] {
		acc_2d_scalar_spec = 0.0f;
		for (int i = 0; i < kBatch; i++)
			acc_2d_scalar_spec += scalar::Perlin2D(xs[i], ys[i]);
	});

	float acc_2d_avx2_spec = 0.0f;
#ifdef __AVX2__
	Stats s_2d_avx2_spec = Bench("avx2_8x", kReps, kBatch, [&] {
		acc_2d_avx2_spec = 0.0f;
		for (int i = 0; i < kBatch; i += 8) {
			__m256 vx = _mm256_loadu_ps(&xs[i]);
			__m256 vy = _mm256_loadu_ps(&ys[i]);
			__m256 r = avx2::Perlin2D_8(vx, vy);
			alignas(32) float buf[8];
			_mm256_store_ps(buf, r);
			for (int j = 0; j < 8; j++)
				acc_2d_avx2_spec += buf[j];
		}
	});
#else
	(void)acc_2d_avx2_spec;
#endif

	// ----------------------------------------------------------------
	// 3D Perlin (spec, perm-table-based)
	// ----------------------------------------------------------------
	std::printf("\n=== 3D Perlin (spec, perm table) ===\n");
	float acc_3d_scalar_spec = 0.0f;
	Stats s_3d_scalar_spec = Bench("scalar", kReps, kBatch, [&] {
		acc_3d_scalar_spec = 0.0f;
		for (int i = 0; i < kBatch; i++)
			acc_3d_scalar_spec += scalar::Perlin3D(xs[i], ys[i], zs[i]);
	});

	float acc_3d_avx2_spec = 0.0f;
#ifdef __AVX2__
	Stats s_3d_avx2_spec = Bench("avx2_8x", kReps, kBatch, [&] {
		acc_3d_avx2_spec = 0.0f;
		for (int i = 0; i < kBatch; i += 8) {
			__m256 vx = _mm256_loadu_ps(&xs[i]);
			__m256 vy = _mm256_loadu_ps(&ys[i]);
			__m256 vz = _mm256_loadu_ps(&zs[i]);
			__m256 r = avx2::Perlin3D_8(vx, vy, vz);
			alignas(32) float buf[8];
			_mm256_store_ps(buf, r);
			for (int j = 0; j < 8; j++)
				acc_3d_avx2_spec += buf[j];
		}
	});
#else
	(void)acc_3d_avx2_spec;
#endif

	// ----------------------------------------------------------------
	// 2D Perlin (SIMD-hash variant)
	// ----------------------------------------------------------------
	std::printf("\n=== 2D Perlin (SIMD-hash variant) ===\n");
	float acc_2d_scalar_simd = 0.0f;
	Stats s_2d_scalar_simd = Bench("scalar", kReps, kBatch, [&] {
		acc_2d_scalar_simd = 0.0f;
		for (int i = 0; i < kBatch; i++)
			acc_2d_scalar_simd += scalar::PerlinSimd2D(xs[i], ys[i]);
	});

	float acc_2d_avx2_simd = 0.0f;
#ifdef __AVX2__
	Stats s_2d_avx2_simd = Bench("avx2_8x", kReps, kBatch, [&] {
		acc_2d_avx2_simd = 0.0f;
		for (int i = 0; i < kBatch; i += 8) {
			__m256 vx = _mm256_loadu_ps(&xs[i]);
			__m256 vy = _mm256_loadu_ps(&ys[i]);
			__m256 r = avx2::PerlinSimd2D_8(vx, vy);
			alignas(32) float buf[8];
			_mm256_store_ps(buf, r);
			for (int j = 0; j < 8; j++)
				acc_2d_avx2_simd += buf[j];
		}
	});
#else
	(void)acc_2d_avx2_simd;
#endif

	// ----------------------------------------------------------------
	// 3D Perlin (SIMD-hash variant)
	// ----------------------------------------------------------------
	std::printf("\n=== 3D Perlin (SIMD-hash variant) ===\n");
	float acc_3d_scalar_simd = 0.0f;
	Stats s_3d_scalar_simd = Bench("scalar", kReps, kBatch, [&] {
		acc_3d_scalar_simd = 0.0f;
		for (int i = 0; i < kBatch; i++)
			acc_3d_scalar_simd += scalar::PerlinSimd3D(xs[i], ys[i], zs[i]);
	});

	float acc_3d_avx2_simd = 0.0f;
#ifdef __AVX2__
	Stats s_3d_avx2_simd = Bench("avx2_8x", kReps, kBatch, [&] {
		acc_3d_avx2_simd = 0.0f;
		for (int i = 0; i < kBatch; i += 8) {
			__m256 vx = _mm256_loadu_ps(&xs[i]);
			__m256 vy = _mm256_loadu_ps(&ys[i]);
			__m256 vz = _mm256_loadu_ps(&zs[i]);
			__m256 r = avx2::PerlinSimd3D_8(vx, vy, vz);
			alignas(32) float buf[8];
			_mm256_store_ps(buf, r);
			for (int j = 0; j < 8; j++)
				acc_3d_avx2_simd += buf[j];
		}
	});
#else
	(void)acc_3d_avx2_simd;
#endif

	// ----------------------------------------------------------------
	// Per-sample output to results.csv
	// ----------------------------------------------------------------
	std::ofstream out("../results.csv");
	out << "variant,dim,reps,batch,mean_us,p95_us,p99_us,stddev_us,min_us,max_us,throughput_M_per_s,acc_sum\n";
	auto row = [&](const char *variant, const char *dim, int reps, int batch,
				   const Stats &s, float throughput_M, float acc_sum) {
		out << variant << "," << dim << "," << reps << "," << batch << ","
			<< s.mean << "," << s.p95 << "," << s.p99 << "," << s.stddev << ","
			<< s.min << "," << s.max << "," << throughput_M << "," << acc_sum << "\n";
	};
	auto thrpt = [&](const Stats &s) { return kBatch / (s.mean * 1e-6) / 1e6; };

	// spec variant
	row("spec_scalar", "2d", kReps, kBatch, s_2d_scalar_spec, thrpt(s_2d_scalar_spec), acc_2d_scalar_spec);
#ifdef __AVX2__
	row("spec_avx2", "2d", kReps, kBatch, s_2d_avx2_spec, thrpt(s_2d_avx2_spec), acc_2d_avx2_spec);
#endif
	row("spec_scalar", "3d", kReps, kBatch, s_3d_scalar_spec, thrpt(s_3d_scalar_spec), acc_3d_scalar_spec);
#ifdef __AVX2__
	row("spec_avx2", "3d", kReps, kBatch, s_3d_avx2_spec, thrpt(s_3d_avx2_spec), acc_3d_avx2_spec);
#endif
	// simd-hash variant
	row("simd_scalar", "2d", kReps, kBatch, s_2d_scalar_simd, thrpt(s_2d_scalar_simd), acc_2d_scalar_simd);
#ifdef __AVX2__
	row("simd_avx2", "2d", kReps, kBatch, s_2d_avx2_simd, thrpt(s_2d_avx2_simd), acc_2d_avx2_simd);
#endif
	row("simd_scalar", "3d", kReps, kBatch, s_3d_scalar_simd, thrpt(s_3d_scalar_simd), acc_3d_scalar_simd);
#ifdef __AVX2__
	row("simd_avx2", "3d", kReps, kBatch, s_3d_avx2_simd, thrpt(s_3d_avx2_simd), acc_3d_avx2_simd);
#endif

	// Correctness checks within each variant.
	std::printf("\nCorrectness (within each variant, expect ~equal):\n");
	std::printf("  spec 2D: scalar=%.6f  avx2=%.6f  rel_err=%.2e\n",
				acc_2d_scalar_spec, acc_2d_avx2_spec,
				std::abs(acc_2d_avx2_spec - acc_2d_scalar_spec) / std::abs(acc_2d_scalar_spec));
	std::printf("  spec 3D: scalar=%.6f  avx2=%.6f  rel_err=%.2e\n",
				acc_3d_scalar_spec, acc_3d_avx2_spec,
				std::abs(acc_3d_avx2_spec - acc_3d_scalar_spec) / std::abs(acc_3d_scalar_spec));
	std::printf("  simd 2D: scalar=%.6f  avx2=%.6f  rel_err=%.2e\n",
				acc_2d_scalar_simd, acc_2d_avx2_simd,
				std::abs(acc_2d_avx2_simd - acc_2d_scalar_simd) / std::abs(acc_2d_scalar_simd));
	std::printf("  simd 3D: scalar=%.6f  avx2=%.6f  rel_err=%.2e\n",
				acc_3d_scalar_simd, acc_3d_avx2_simd,
				std::abs(acc_3d_avx2_simd - acc_3d_scalar_simd) / std::abs(acc_3d_scalar_simd));

	// Speedup table.
	std::printf("\nSpeedup (avx2 / scalar per-sample throughput, >1 means faster):\n");
#ifdef __AVX2__
	auto sp = [&](const Stats &sc, const Stats &av) {
		double ts = thrpt(sc), ta = thrpt(av);
		return std::pair<double, double>{ta / ts, ta};
	};
	auto p = sp(s_2d_scalar_spec, s_2d_avx2_spec);
	std::printf("  spec  2D: %.2fx (scalar %.1f M/s, avx2 %.1f M/s)\n", p.first, thrpt(s_2d_scalar_spec), p.second);
	p = sp(s_3d_scalar_spec, s_3d_avx2_spec);
	std::printf("  spec  3D: %.2fx (scalar %.1f M/s, avx2 %.1f M/s)\n", p.first, thrpt(s_3d_scalar_spec), p.second);
	p = sp(s_2d_scalar_simd, s_2d_avx2_simd);
	std::printf("  simd  2D: %.2fx (scalar %.1f M/s, avx2 %.1f M/s)\n", p.first, thrpt(s_2d_scalar_simd), p.second);
	p = sp(s_3d_scalar_simd, s_3d_avx2_simd);
	std::printf("  simd  3D: %.2fx (scalar %.1f M/s, avx2 %.1f M/s)\n", p.first, thrpt(s_3d_scalar_simd), p.second);
#endif

	return 0;
}