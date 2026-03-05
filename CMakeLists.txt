cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(ProjectV
        VERSION 0.0.1
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#----------------------------------------------------------

add_executable(ProjectV
        src/main.cpp
        # Add new files here
)

target_sources(ProjectV
        PRIVATE
        src/main.cpp
)

target_compile_options(ProjectV PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
)

target_link_libraries(ProjectV PRIVATE
        # Add new libs here
)
