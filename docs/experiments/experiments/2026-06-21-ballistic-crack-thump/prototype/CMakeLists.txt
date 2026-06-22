cmake_minimum_required(VERSION 3.20)
project(ballistic_audio_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ballistic_audio_bench ballistic_audio_bench.cpp)

target_compile_options(ballistic_audio_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
