cmake_minimum_required(VERSION 3.30)
project(tank_suspension_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tank_suspension_bench tank_suspension_bench.cpp)
target_compile_options(tank_suspension_bench PRIVATE
  -O3 -march=native -mtune=native
  -ffast-math -fno-exceptions -fno-rtti
  -DNDEBUG
)
target_link_options(tank_suspension_bench PRIVATE -flto=thin)
