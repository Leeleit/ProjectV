// **Tier 2.A probe (`2026-06-13`).** Tiny TU that imports
// `projectv.probe` and prints the value. Used to verify the
// CMake `FILE_SET CXX_MODULES` + `CMAKE_CXX_SCAN_FOR_MODULES=ON`
// pipeline works end-to-end before we convert the real `Math.ixx`.
//
// The `.cpp` deliberately does NOT `#include "core/Probe.ixx"` —
// it `import`s the module. If the build succeeds and prints
// `42`, the pipeline is healthy.
import projectv.probe;

#include <cstdio>

int main()
{
	std::printf("probe: %d\n", projectv::probe::kProbeValue);
	return 0;
}
