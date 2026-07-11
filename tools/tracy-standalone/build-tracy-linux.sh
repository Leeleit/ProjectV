#!/usr/bin/env bash
# **Tracy Profiler UI standalone build — Linux (`2026-06-15`).**
#
# Configures and builds the Tracy UI as a separate top-level CMake project
# at external/tracy/profiler. On Linux, Tracy UI's tidy-html5 dependency
# (CPMAddPackage tidy-html5 5.8.0) uses removed `uint` / `ulong` types
# that broke glibc 2.36+, so this build may fail at the tidy-static
# compilation step on modern Linux. This is a known upstream
# wolfpld/tracy bug (see `agent/knowledge.md`). On Windows the same
# upstream code compiles fine.
#
# Usage:
#   bash tools/tracy-standalone/build-tracy-linux.sh
#   bash tools/tracy-standalone/build-tracy-linux.sh build/linux-clang-tracy
#   bash tools/tracy-standalone/build-tracy-linux.sh build/linux-clang-tracy --configure-only
#   bash tools/tracy-standalone/build-tracy-linux.sh build/linux-clang-tracy --compiler clang++

set -euo pipefail

BUILD_DIR="build/linux-clang-tracy"
COMPILER="clang++"
CONFIGURE_ONLY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --compiler)  COMPILER="$2"; shift 2 ;;
        --configure-only) CONFIGURE_ONLY=1; shift ;;
        -h|--help)
            sed -n '2,15p' "$0"
            exit 0
            ;;
        -*)
            echo "Unknown flag: $1" >&2
            exit 1
            ;;
        *)
            BUILD_DIR="$1"; shift ;;
    esac
done

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$( cd "$SCRIPT_DIR/../.." && pwd )"
TRACY_PROFILER_SRC="$REPO_ROOT/external/tracy/profiler"
BUILD_DIR_FULL="$REPO_ROOT/$BUILD_DIR"
CPM_SOURCE_CACHE="$REPO_ROOT/build/cpm-source-cache"

if [[ ! -d "$TRACY_PROFILER_SRC" ]]; then
    echo "Tracy profiler source not found at: $TRACY_PROFILER_SRC" >&2
    exit 1
fi

if ! command -v "$COMPILER" >/dev/null 2>&1; then
    echo "Compiler '$COMPILER' not found in PATH. Install clang 22+ and ensure it's reachable, or pass --compiler <full-path>." >&2
    exit 1
fi

echo "[tracy-standalone] Tracy profiler source : $TRACY_PROFILER_SRC"
echo "[tracy-standalone] Build dir             : $BUILD_DIR_FULL"
echo "[tracy-standalone] CPM source cache      : $CPM_SOURCE_CACHE"
echo "[tracy-standalone] Compiler              : $COMPILER"
echo "[tracy-standalone] Note                  : Tracy UI build on Linux may fail at tidy-html5 step"
echo "                                              (upstream wolfpld/tracy bug — see $(agent/knowledge.md))."

# Configure. Tracy profiler's CMakeLists.txt has its own project()
# call (project(tracy-profiler)), so we can't add_subdirectory it
# from a parent CMakeLists. The standalone build is the canonical
# way to compile the Tracy UI without colliding with ProjectV's
# own nlohmann_json FetchContent target (CMP0002).
CONFIGURE_ARGS=(
    -S        "$TRACY_PROFILER_SRC"
    -B        "$BUILD_DIR_FULL"
    -G        "Ninja"
    -DCMAKE_C_COMPILER="$COMPILER"
    -DCMAKE_CXX_COMPILER="$COMPILER"
    -DCMAKE_CXX_COMPILER_LAUNCHER="sccache"
    -DCMAKE_BUILD_TYPE=Release
    -DPROJECTV_BUILD_TRACY_PROFILER=ON
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_DISABLE_FIND_PACKAGE_rocprofiler-sdk=TRUE
    -DCPM_SOURCE_CACHE="$CPM_SOURCE_CACHE"
    -DNO_ISA_EXTENSIONS=ON
    -DBASE64_WERROR=OFF
    -DBUILD_LIBCURL_DOCS=OFF
    -DBUILD_MISC_DOCS=OFF
    -DENABLE_CURL_MANUAL=OFF
    -DCMAKE_MESSAGE_LOG_LEVEL=NOTICE
    -DCMAKE_WARN_DEPRECATED=OFF
    -DCMAKE_SUPPRESS_DEVELOPER_WARNINGS=TRUE
    -DCMAKE_POLICY_DEFAULT_CMP0069=NEW
)

echo "[tracy-standalone] Configuring..."
cmake "${CONFIGURE_ARGS[@]}"

if [[ "$CONFIGURE_ONLY" -eq 1 ]]; then
    echo "[tracy-standalone] Configure-only mode; skipping build."
    exit 0
fi

echo "[tracy-standalone] Building (tracy-profiler + tracy-capture)..."
cmake --build "$BUILD_DIR_FULL" --target tracy-profiler tracy-capture

echo ""
echo "[tracy-standalone] Done. Run the Tracy UI to connect to ProjectV:"
echo "  $BUILD_DIR_FULL/tracy-profiler"
echo ""
echo "[tracy-standalone] ProjectV must have been built with PROJECTV_ENABLE_TRACY=ON"
echo "[tracy-standalone] (use cmake --preset linux-clang-debug-tracy-profiler)."
