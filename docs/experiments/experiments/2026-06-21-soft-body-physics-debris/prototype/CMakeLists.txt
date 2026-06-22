cmake_minimum_required(VERSION 4.0)
project(soft_body_debris_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Per `docs/experiments/AGENTS.md §1` — build dir inside prototype/
# (not in repo root or shared `build/`).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(soft_body_debris_bench soft_body_debris_bench.cpp)

# Per `benchmarks/methodology.md §2` + `hardware-profile.md §6`:
# Clang 22.1.6, -O3 -march=native -DNDEBUG, strict warnings.
target_compile_options(soft_body_debris_bench PRIVATE
    -O3 -march=native -DNDEBUG
    -Wall -Wextra -Wpedantic
)

# Per `hardware-profile.md §6`: LLD linker.
set_target_properties(soft_body_debris_bench PROPERTIES
    LINKER_TYPE LLD
)
