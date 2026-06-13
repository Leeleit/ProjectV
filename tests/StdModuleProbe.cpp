// **Tier 2.C probe (`2026-06-13`).** Tiny TU that
// `import std;` and uses one `std::vector` to validate
// the C++23+ standard-library module import pipeline on
// the current toolchain. Per the agent notes, libstdc++
// 16.1.1 doesn't ship a precompiled `std` module; if
// this probe fails to compile, the failure is informative
// (it surfaces the Clang error which names the missing
// `.cppm`). Per `TODO.md Tier 2.C`, the CMake 4.2+
// experimental gate
// (`CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
// d0edc3af-4c50-42ea-a356-e2862fe7a444`) is what's
// needed to enable `import std;` in the first place; this
// probe is the build-side half of the validation.
import std;

#include <cstdio>

int main()
{
	std::vector<int> v{1, 2, 3, 4, 5};
	int sum = 0;
	for (int x : v) {
		sum += x;
	}
	std::printf("std module probe: sum=%d (expected 15)\n", sum);
	return sum == 15 ? 0 : 1;
}
