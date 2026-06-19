module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

export module projectv.string_id;

export namespace projectv::core {

struct alignas(16) StringID {
	std::uint64_t hash{};
	std::uint32_t length{};
	std::uint32_t _pad{};

	// **FNV-1a 64-bit basis.** Per
	// http://www.isthe.com/chongo/tech/comp/fnv/. The basis
	// and prime are baked into constexpr helpers so the
	// hash is computed entirely at compile time for
	// `constexpr` callers.
	static constexpr std::uint64_t kFnv1aOffsetBasis = 0xcbf29ce484222325ULL;
	static constexpr std::uint64_t kFnv1aPrime = 0x100000001b3ULL;

	constexpr StringID() noexcept = default;

	// **Compile-time ctor for string literals.** Resolves
	// to a single `mov` of the precomputed hash at the
	// call site; no init code emitted, no `.rodata`
	// string lookup.
	template <std::size_t N>
		consteval StringID(const char (&literal)[N]) noexcept
		: StringID(std::string_view{literal, N - 1}) {}

	// **Runtime ctor for `std::string_view`.** Used by
	// env-var parsers (`ParseAssetManifestString`),
	// file loaders, and any path that doesn't have a
	// literal at the call site. The hash is identical to
	// the literal ctor for identical bytes, so
	// `StringID("rock")` from env equals
	// `StringID("rock")` literal.
	constexpr StringID(std::string_view view) noexcept
		: hash(computeHash(view)), length(static_cast<std::uint32_t>(view.size())) {}

	// **Hashing helper.** Public so callers (e.g.
	// `std::hash<StringID>`) don't have to inline the
	// FNV-1a arithmetic themselves.
	static constexpr std::uint64_t computeHash(std::string_view view) noexcept {
		std::uint64_t h = kFnv1aOffsetBasis;
		for (char c : view) {
			h ^= static_cast<std::uint8_t>(c);
			h *= kFnv1aPrime;
		}
		return h;
	}

	// **Reverse mapping.** Linear-scans a static table of
	// literals for a matching `(hash, length)` tuple.
	// Returns the literal on hit, or `nullptr` (or a
	// fallback) on miss. Intended for UI / logging
	// only — the hot path uses `operator==` and never
	// needs the original string.
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

// **Specialise `std::hash<StringID>`** so the type can be
// used directly as `std::unordered_map<StringID, T>::key_type`
// without a custom hasher.
//
// NOTE on `export namespace std`: C++20 modules allow
// `export namespace std { ... }` to add declarations into
// the standard library's namespace from a module. Clang
// 22 accepts this pattern. The `template<>` is required
// to make the specialisation distinguishable from the
// primary template.
export namespace std {
template <>
struct hash<projectv::core::StringID> {
	constexpr std::size_t operator()(const projectv::core::StringID &id) const noexcept {
		return static_cast<std::size_t>(id.hash ^ (static_cast<std::uint64_t>(id.length) << 32));
	}
};
} // namespace std
