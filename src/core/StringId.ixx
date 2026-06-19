module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module projectv.string_id;

export namespace projectv::core {

struct alignas(16) StringID {
	std::uint64_t hash{};
	std::uint32_t length{};
	std::uint32_t _pad{};

	static constexpr std::uint64_t kFnv1aOffsetBasis = 0xcbf29ce484222325ULL;
	static constexpr std::uint64_t kFnv1aPrime = 0x100000001b3ULL;

	constexpr StringID() noexcept = default;

	template <std::size_t N>
	consteval explicit StringID(const char (&literal)[N]) noexcept
		: StringID(std::string_view{literal, N - 1}) {}

	constexpr explicit StringID(const std::string_view view) noexcept
		: hash(computeHash(view)), length(static_cast<std::uint32_t>(view.size())) {}

	static constexpr std::uint64_t computeHash(const std::string_view view) noexcept
	{
		std::uint64_t h = kFnv1aOffsetBasis;
		for (const char c : view) {
			h ^= static_cast<std::uint8_t>(c);
			h *= kFnv1aPrime;
		}
		return h;
	}

	template <std::size_t N>
	static constexpr const char *toView(const StringID &id, const std::array<const char *, N> &table) noexcept
	{
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

constexpr bool operator==(const StringID &a, const StringID &b) noexcept
{
	return a.hash == b.hash && a.length == b.length;
}
constexpr bool operator!=(const StringID &a, const StringID &b) noexcept
{
	return !(a == b);
}
constexpr bool operator<(const StringID &a, const StringID &b) noexcept
{
	if (a.hash != b.hash) {
		return a.hash < b.hash;
	}
	return a.length < b.length;
}

} // namespace projectv::core

export template <>
struct std::hash<projectv::core::StringID> {
	constexpr std::size_t operator()(const projectv::core::StringID &id) const noexcept
	{
		return id.hash ^ static_cast<std::uint64_t>(id.length) << 32;
	}
}; // namespace std
