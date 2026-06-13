#ifndef PROJECTV_CORE_STRING_ID_HPP
#define PROJECTV_CORE_STRING_ID_HPP

// **Tier 1.D (`2026-06-13`).** Cheap, hashable, compile-time-friendly
// identifier for hot-path string equality / hash-map keys.
//
// Replaces `std::string` in hot-path id fields (e.g.
// `ModelRegistryEntry::id`, `ManifestEntry::id`, audio track / artist /
// title tags) where the only operations needed are: equality compare,
// hash-map key, and re-resolve back to a human-readable string for UI
// / logging. Per `legacy/docs/philosophy/02_paradigms/06_strings-philosophy.md`
// `std::string` in hot path is forbidden: it allocates, it dangles, it
// doesn't fit in a single cache line, and it forces `unordered_map` to
// hash the bytes every time.
//
// **Layout** (12 B, 4 B padding to 16 B for 16-byte SIMD lanes if we
// ever want to compare 4 StringIDs at once):
//
//   - `hash: uint64_t` — FNV-1a 64-bit, computed at compile time
//     for `constexpr` callers, or at construction time for runtime
//     callers (env-var parsing, file I/O).
//   - `length: uint32_t` — byte length of the source string, stored
//     for two reasons:
//       (1) collision mitigation: FNV-1a is 64-bit, so collisions
//           for short ASCII strings are astronomically unlikely, but
//           we still need to detect them — `length` is part of the
//           equality tuple so two ids with the same hash but
//           different lengths will compare unequal.
//       (2) `string_view()` reconstruction without allocating.
//   - `_pad: uint32_t` — explicit padding to 16 B so the struct
//       lives on a 16-byte boundary (same alignment as
//       `projectv::math::Vec4`); no implicit padding so the layout
//       is portable across MSVC / clang / GCC.
//
// **Two ctors, two phases:**
//
//   - `constexpr StringID(const char (&literal)[N])` — compile-time
//     ctor for string literals. Hashes the literal at compile time,
//     so `static const StringID kRockDiffuse = "rock_diffuse";` at
//     namespace scope is zero-cost (no runtime init, no string in
//     `.rodata` lookup, just a `mov` of the precomputed hash).
//   - `constexpr StringID(std::string_view view)` — runtime ctor
//     for env-var-parsed strings, file-loaded tags, etc. Computes
//     FNV-1a over the bytes; identical hash to the literal ctor
//     for the same bytes (so `StringID("rock_diffuse")` from env
//     equals `StringID("rock_diffuse")` literal at compile time).
//
// **Equality** compares the full 12-byte tuple `(hash, length)`.
// `hash` alone is not enough: FNV-1a is short (64 bits) and on
// pathological inputs (very long strings, hostile crafted inputs)
// could collide. Length disambiguates without storing the actual
// string. For ASCII-only inputs with reasonable lengths (< 64 chars)
// the hash is effectively unique.
//
// **Reverse mapping** via `to_view(const char *table[])` is provided
// as a static helper for callers that need the human-readable form
// (UI, logging). The operator passes a static array of literals; the
// function linear-scans for a matching `(hash, length)` and returns
// the original literal. For the typical "few dozen known ids" use
// case the linear scan is faster than `unordered_map<std::string, ...>`.
//
// **No allocation, no `std::string` member, no virtual, trivially
// copyable.** Passes through registers, lives in `.rodata` for
// constexpr values.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace projectv::core {

struct alignas(16) StringID {
	std::uint64_t hash{};
	std::uint32_t length{};
	std::uint32_t _pad{};

	// **FNV-1a 64-bit basis.** Per http://www.isthe.com/chongo/tech/comp/fnv/.
	// The basis and prime are baked into constexpr helpers so the
	// hash is computed entirely at compile time for `constexpr`
	// callers.
	static constexpr std::uint64_t kFnv1aOffsetBasis = 0xcbf29ce484222325ULL;
	static constexpr std::uint64_t kFnv1aPrime = 0x100000001b3ULL;

	constexpr StringID() noexcept = default;

	// **Compile-time ctor for string literals.** Resolves to a single
	// `mov` of the precomputed hash at the call site; no init code
	// emitted, no `.rodata` string lookup.
	template <std::size_t N>
	consteval StringID(const char (&literal)[N]) noexcept
		: StringID(std::string_view{literal, N - 1}) {}

	// **Runtime ctor for `std::string_view`.** Used by env-var
	// parsers (`ParseAssetManifestString`), file loaders, and any
	// path that doesn't have a literal at the call site. The hash
	// is identical to the literal ctor for identical bytes, so
	// `StringID("rock")` from env equals `StringID("rock")` literal.
	constexpr StringID(std::string_view view) noexcept
		: hash(computeHash(view)), length(static_cast<std::uint32_t>(view.size())) {}

	// **Hashing helper.** Public so callers (e.g. `std::hash<StringID>`
	// specialisation) don't have to inline the FNV-1a arithmetic
	// themselves.
	static constexpr std::uint64_t computeHash(std::string_view view) noexcept {
		std::uint64_t h = kFnv1aOffsetBasis;
		for (char c : view) {
			h ^= static_cast<std::uint8_t>(c);
			h *= kFnv1aPrime;
		}
		return h;
	}

	// **Reverse mapping.** Linear-scans a static table of literals
	// for a matching `(hash, length)` tuple. Returns the literal
	// on hit, or `nullptr` (or a fallback) on miss. Intended for
	// UI / logging only — the hot path uses `operator==` and never
	// needs the original string.
	//
	// The `table` array is templated on size so the compiler can
	// unroll the loop and constant-fold the length compares for
	// small N. For the typical "10-30 known ids" the cost is
	// negligible.
	template <std::size_t N>
	static constexpr const char *toView(const StringID &id, const std::array<const char *, N> &table) noexcept {
		for (const char *literal : table) {
			const std::size_t literalLen = std::char_traits<char>::length(literal);
			if (literalLen == id.length && StringID{std::string_view{literal, literalLen}}.hash == id.hash) {
				return literal;
			}
		}
		return nullptr;
	}
};

static_assert(sizeof(StringID) == 16, "StringID must be 16 bytes");
static_assert(alignof(StringID) == 16, "StringID must be 16-byte aligned");
static_assert(std::is_trivially_copyable_v<StringID>);

constexpr bool operator==(const StringID &a, const StringID &b) noexcept {
	return a.hash == b.hash && a.length == b.length;
}
constexpr bool operator!=(const StringID &a, const StringID &b) noexcept { return !(a == b); }
constexpr bool operator<(const StringID &a, const StringID &b) noexcept {
	if (a.hash != b.hash) {
		return a.hash < b.hash;
	}
	return a.length < b.length;
}

} // namespace projectv::core

// **Specialise `std::hash<StringID>`** so the type can be used
// directly as `std::unordered_map<StringID, T>::key_type` without
// a custom hasher.
namespace std {
template <>
struct hash<projectv::core::StringID> {
	constexpr std::size_t operator()(const projectv::core::StringID &id) const noexcept {
		// Mix hash and length so a 32-bit `size_t` (e.g. on
		// 32-bit ABIs) still produces distinct values for
		// `(hash=H, length=L1)` and `(hash=H, length=L2)`.
		// On 64-bit, this is a no-op xor of the high bits.
		return static_cast<std::size_t>(id.hash ^ (static_cast<std::uint64_t>(id.length) << 32));
	}
};
} // namespace std

#endif // PROJECTV_CORE_STRING_ID_HPP
