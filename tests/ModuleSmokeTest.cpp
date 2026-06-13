// **Tier 2.A probe (`2026-06-13`).** Tiny TU that
// `import projectv.probe;` and prints the value. Validates
// the CMake `FILE_SET CXX_MODULES` + `CMAKE_CXX_SCAN_FOR_MODULES=ON`
// pipeline end-to-end before we commit to converting the
// real `Math.ixx`.
//
// If this executable builds and prints `42`, the modules
// pipeline is healthy. If the `import` fails, the build
// will surface the Clang error (the most common is "module
// 'projectv.probe' not found" if the `FILE_SET` is wired
// wrong, or "module declaration must occur at the start of
// the translation unit" if a global preprocessor flag is
// interfering with the `.ixx`'s `export module X;`).
import projectv.probe;

#include <cstdio>

int main()
{
	std::printf("probe: %d\n", projectv::probe::kProbeValue);
	return 0;
}
