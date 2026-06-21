#pragma once

#include <cstdio>
#include <string_view>

struct TestContext {
	int failures = 0;

	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

template <typename T>
void ExpectEqual(TestContext &context, const T &expected, const T &actual, const int line, const std::string_view expr)
{
	if (!(expected == actual)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.*s (expected %lld, got %lld)",
			static_cast<int>(expr.size()),
			expr.data(),
			static_cast<long long>(expected),
			static_cast<long long>(actual));
		context.Fail(line, buffer);
	}
}

inline void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

#define EXPECT_TRUE(context, expr) ExpectTrue(context, (expr), __LINE__, #expr)
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)