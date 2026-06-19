#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "core/StringId.hpp"

namespace {

using projectv::core::StringID;

int g_testsPassed = 0;
int g_testsFailed = 0;

#define VERIFY(expr) do { \
	if (expr) { \
		++g_testsPassed; \
	} else { \
		++g_testsFailed; \
		std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
	} \
} while (0)

#define VERIFY_EQ(a, b) do { \
	const auto _a = (a); \
	const auto _b = (b); \
	if (_a == _b) { \
		++g_testsPassed; \
	} else { \
		++g_testsFailed; \
		std::fprintf(stderr, "FAIL %s:%d  %s == %s  (got %lld vs %lld)\n", \
			__FILE__, __LINE__, #a, #b, \
			static_cast<long long>(_a), static_cast<long long>(_b)); \
	} \
} while (0)

void VerifyLayout() {
	VERIFY(sizeof(StringID) == 16);
	VERIFY(alignof(StringID) == 16);
	VERIFY(std::is_trivially_copyable_v<StringID>);
	VERIFY(std::is_standard_layout_v<StringID>);
	const StringID kDefault{};
	VERIFY_EQ(kDefault.hash, 0ULL);
	VERIFY_EQ(kDefault.length, 0U);
}

void VerifyConstexprLiteralCtor() {

	constexpr StringID kFoobar{"foobar"};
	VERIFY_EQ(kFoobar.hash, 0x85944171f73967e8ULL);
	VERIFY_EQ(kFoobar.length, 6U);
	constexpr StringID kEmpty{""};
	VERIFY_EQ(kEmpty.hash, StringID::kFnv1aOffsetBasis);
	VERIFY_EQ(kEmpty.length, 0U);
	constexpr StringID kA{"a"};
	const std::uint64_t expectedA = (StringID::kFnv1aOffsetBasis ^ 0x61) * StringID::kFnv1aPrime;
	VERIFY_EQ(kA.hash, expectedA);
	VERIFY_EQ(kA.length, 1U);
}

void VerifyRuntimeStringViewCtor() {

	const std::string_view runtimeView{"rock_diffuse"};
	const StringID runtimeID{runtimeView};
	const StringID literalID{"rock_diffuse"};
	VERIFY(runtimeID == literalID);
	VERIFY_EQ(runtimeID.hash, literalID.hash);
	VERIFY_EQ(runtimeID.length, literalID.length);
	const std::string s{"rock_diffuse"};
	const StringID fromString{std::string_view{s}};
	VERIFY(fromString == literalID);
	const StringID partial{std::string_view{"rock_"}};
	VERIFY(partial != literalID);
	VERIFY_EQ(partial.length, 5U);
}

void VerifyFnv1aKnownVectors() {

	const StringID emptyStr{std::string_view{""}};
	VERIFY_EQ(emptyStr.hash, 0xcbf29ce484222325ULL);
	const StringID aStr{std::string_view{"a"}};
	VERIFY_EQ(aStr.hash, 0xaf63dc4c8601ec8cULL);
	const StringID foobarStr{std::string_view{"foobar"}};
	VERIFY_EQ(foobarStr.hash, 0x85944171f73967e8ULL);
}

void VerifyEqualityAndOrdering() {
	const StringID a{"alpha"};
	const StringID a2{"alpha"};
	const StringID b{"beta"};
	VERIFY(a == a2);
	VERIFY(!(a != a2));
	VERIFY(a != b);
	const StringID a3{"alphb"};
	VERIFY(a != a3);
	VERIFY_EQ(a.length, a3.length);
	VERIFY(a.hash != a3.hash);
	VERIFY((a < b) || (b < a));
	VERIFY(!(a < a2));
}

void VerifyStdHashSpecialisation() {

	const StringID a{"hashable_id"};
	const StringID a2{"hashable_id"};
	const std::hash<StringID> hasher{};
	VERIFY(hasher(a) == hasher(a2));
	std::unordered_map<StringID, int> m;
	m[StringID{"one"}] = 1;
	m[StringID{"two"}] = 2;
	m[StringID{"three"}] = 3;
	VERIFY_EQ(m.at(StringID{"one"}), 1);
	VERIFY_EQ(m.at(StringID{"two"}), 2);
	VERIFY_EQ(m.at(StringID{"three"}), 3);
	const auto it = m.find(StringID{"nope"});
	VERIFY(it == m.end());
}

void VerifyToViewReverseMapping() {

	const std::array<const char *, 3> kTable{"rock_diffuse", "metal_rusty", "wood_oak"};
	const StringID rock{"rock_diffuse"};
	const StringID metal{"metal_rusty"};
	const StringID oak{"wood_oak"};
	const StringID unknown{std::string_view{"unknown_id"}};
	VERIFY(std::strcmp(StringID::toView(rock, kTable), "rock_diffuse") == 0);
	VERIFY(std::strcmp(StringID::toView(metal, kTable), "metal_rusty") == 0);
	VERIFY(std::strcmp(StringID::toView(oak, kTable), "wood_oak") == 0);
	VERIFY(StringID::toView(unknown, kTable) == nullptr);
	const std::array<const char *, 0> kEmptyTable{};
	VERIFY(StringID::toView(rock, kEmptyTable) == nullptr);
	const StringID kDefault{};
	VERIFY(StringID::toView(kDefault, kTable) == nullptr);
}

void VerifyEmptyString() {
	const StringID emptyLit{""};
	const StringID emptyView{std::string_view{""}};
	VERIFY(emptyLit == emptyView);
	VERIFY_EQ(emptyLit.length, 0U);
	const StringID partial{std::string_view{""}};
	VERIFY(partial == emptyLit);
	const StringID aStr{"a"};
	VERIFY(aStr != emptyLit);
	VERIFY_EQ(aStr.length, 1U);
	VERIFY(aStr.hash != emptyLit.hash);
}

} // namespace

int main() {
	VerifyLayout();
	VerifyConstexprLiteralCtor();
	VerifyRuntimeStringViewCtor();
	VerifyFnv1aKnownVectors();
	VerifyEqualityAndOrdering();
	VerifyStdHashSpecialisation();
	VerifyToViewReverseMapping();
	VerifyEmptyString();
	std::printf("ProjectVStringIdTests: %d/%d passed (%d failed)\n",
		g_testsPassed, g_testsPassed + g_testsFailed, g_testsFailed);
	return g_testsFailed == 0 ? 0 : 1;
}
