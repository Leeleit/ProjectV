cmake_minimum_required(VERSION 3.20)
project(defrag_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(defrag_bench defrag_bench.cpp)

target_compile_options(defrag_bench PRIVATE
    -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
)

# Output dir for results.csv.
set_target_properties(defrag_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
