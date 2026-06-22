# Standalone C++26 CPU prototype for MAD ASW axis.
# Not part of mainline build (per `agent/AGENTS.md §1` protocol: research prototype only).
# Per docs/experiments/benchmarks/methodology.md §3 measurement protocol.
#
# Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic mad_asw_bench.cpp -o build/mad_asw_bench
# Run:   ./build/mad_asw_bench  (writes build/results.csv + build/summary_means.csv + build/run.log)
#
# Reproduces: 5 strategies × 5 ASW scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
# Wall time: ~0.13 sec на Zen 3 5800X governor=performance per docs/experiments/hardware-profile.md §1.

cmake_minimum_required(VERSION 3.20)
project(mad_asw_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(mad_asw_bench mad_asw_bench.cpp)

target_compile_options(mad_asw_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
