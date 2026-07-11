# Tracy Profiler UI — standalone build

`ProjectV` ships with Tracy **instrumentation** in `ProjectV.exe` /
`ProjectV` ELF when built with `PROJECTV_ENABLE_TRACY=ON`. The
**Tracy UI** (`tracy-profiler` / `tracy-capture`) is a separate
application that connects over TCP to read the captured data and
visualize it.

The Tracy UI build is **deliberately not** wired into the main
ProjectV CMake scope. Tracy profiler's `CMakeLists.txt` calls
`project(tracy-profiler)` itself, so it can't be `add_subdirectory`'d
from a parent project, and Tracy's `cmake/vendor.cmake` pulls
`nlohmann/json v3.12.0` via CPMAddPackage which collides with
ProjectV's own `FetchContent` `nlohmann_json v3.11.3` target
(CMP0002 — `add_library cannot create target "nlohmann_json"`).

This directory contains **thin wrapper scripts** that invoke
`cmake -S external/tracy/profiler -B build/<dir>` directly,
so Tracy profiler UI is built in its own top-level project scope
where ProjectV's FetchContent target doesn't exist.

## Why a separate scope?

| Concern                                                         | ProjectV scope                                                                                                            | Tracy-only scope                                     |
|-----------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
| `add_subdirectory(external/tracy/profiler)`                     | would call `project(tracy-profiler)` again → CMake error                                                                  | n/a (it's the top-level project)                     |
| Tracy `vendor.cmake` `CPMAddPackage(NAME json ...)`             | collides with `nlohmann_json::nlohmann_json` already created by `FetchContent_MakeAvailable` at root `CMakeLists.txt:475` | no conflict — no prior `nlohmann_json` target exists |
| `tidy-html5` upstream bug (uses removed `uint` / `ulong` types) | only matters on Linux/glibc 2.36+; on Windows compiles fine                                                               | n/a — Tracy UI is built directly here                |
| Tracy UI build cost (5-10 min configure, 200+ MB CPM cache)     | bloats every ProjectV configure                                                                                           | pays only when operator actually wants Tracy UI      |

## Workflow

1. **Build ProjectV with Tracy instrumentation** (no UI):
   ```bash
   cmake --preset windows-clang-debug-tracy-profiler
   cmake --build build/windows-clang-debug-tracy-profiler --target ProjectV
   ```
   This produces `ProjectV.exe` with Tracy symbols embedded; launch it
   and run the workload you want to profile.

2. **Build Tracy UI standalone** (in a separate build dir):
   - **Windows:**
     ```powershell
     .\tools\tracy-standalone\build-tracy-windows.ps1 -BuildDir build\windows-clang-tracy
     ```
   - **Linux:**
     ```bash
     bash tools/tracy-standalone/build-tracy-linux.sh build/linux-clang-tracy
     ```

3. **Launch Tracy UI** and connect to ProjectV:
   - **Windows:** `build\windows-clang-tracy\bin\tracy-profiler.exe`
   - **Linux:** `build/linux-clang-tracy/tracy-profiler`
   - Click **Connect** (default `127.0.0.1:8086`). The Tracy UI will
     show live capture if `ProjectV.exe` is still running, or you can
     load a `.tracy` capture file saved via the UI.

## Why not a CMake preset?

CMake presets v1...v10 (`cmake --help-manual cmake-presets` lists
the full schema) **do not** allow `sourceDir` to be set inside a
configure preset — `${sourceDir}` is a read-only macro resolving
to the directory containing `CMakePresets.json`. The only way to
build a subproject (like `external/tracy/profiler`) is via the
`cmake -S <path>` command-line flag, which the wrapper scripts
in this directory provide.

## Tracy UI build options

The wrapper scripts pass the same cache variables as the
`windows-clang-debug-tracy-profiler` preset to keep Tracy UI
behavior consistent across the two scopes:

| Variable                                                            | Value                                 | Why                                                                                                                                             |
|---------------------------------------------------------------------|---------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| `PROJECTV_BUILD_TRACY_PROFILER`                                     | `ON`                                  | Required by Tracy profiler UI's own `CMakeLists.txt`                                                                                            |
| `CMAKE_POLICY_VERSION_MINIMUM`                                      | `3.5`                                 | Tracy 0.13 still uses pre-CMP0076 policies in its `tidy-html5` patch                                                                            |
| `CPM_SOURCE_CACHE`                                                  | `${sourceDir}/build/cpm-source-cache` | Shared with ProjectV's main build to avoid re-downloading capstone / glfw / libcurl / freetype / pugixml / md4c / nfd / usearch / tidy / base64 |
| `NO_ISA_EXTENSIONS` / `BASE64_WERROR` / `BUILD_LIBCURL_DOCS` / etc. | OFF                                   | All standard Tracy-UI switches to keep its build slim and warning-clean                                                                         |

The `PROJECTV_ENABLE_TRACY=ON` flag is **not** required for the
Tracy UI build (it controls the **client** library instrumentation
in ProjectV; Tracy UI is the consumer of that data, not the
producer).
