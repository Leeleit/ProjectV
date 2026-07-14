cmake_minimum_required(VERSION 3.16)

project(tracy-capture-cli LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TRACY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/tracy")

# TracyServer requires capstone, zstd, ppqsort. Use system zstd; download
# capstone 6.0.0-Alpha9 (matches Tracy's vendor.cmake) and ppqsort, which are
# not reliably compatible/packaged on common distros.
find_package(PkgConfig REQUIRED)

include(FetchContent)
FetchContent_Declare(
    capstone
    URL https://github.com/capstone-engine/capstone/archive/refs/tags/6.0.0-Alpha9.tar.gz
)
FetchContent_MakeAvailable(capstone)
add_library(TracyCapstone INTERFACE)
target_include_directories(TracyCapstone INTERFACE ${capstone_SOURCE_DIR}/include/capstone)
target_link_libraries(TracyCapstone INTERFACE capstone_static)

pkg_check_modules(ZSTD REQUIRED libzstd)
add_library(libzstd INTERFACE)
target_include_directories(libzstd INTERFACE ${ZSTD_INCLUDE_DIRS})
target_link_libraries(libzstd INTERFACE ${ZSTD_LINK_LIBRARIES})

FetchContent_Declare(
    ppqsort
    URL https://github.com/GabTux/PPQSort/archive/refs/tags/v1.0.6.tar.gz
)
FetchContent_GetProperties(ppqsort)
if(NOT ppqsort_POPULATED)
    FetchContent_Populate(ppqsort)
endif()
add_library(PPQSort INTERFACE)
target_include_directories(PPQSort INTERFACE ${ppqsort_SOURCE_DIR}/include)
add_library(PPQSort::PPQSort ALIAS PPQSort)

set(TRACY_COMMON_DIR "${TRACY_ROOT}/public/common")
set(TRACY_COMMON_SOURCES
    ${TRACY_COMMON_DIR}/tracy_lz4.cpp
    ${TRACY_COMMON_DIR}/tracy_lz4hc.cpp
    ${TRACY_COMMON_DIR}/TracySocket.cpp
    ${TRACY_COMMON_DIR}/TracyStackFrames.cpp
    ${TRACY_COMMON_DIR}/TracySystem.cpp)

set(TRACY_SERVER_DIR "${TRACY_ROOT}/server")
set(TRACY_SERVER_SOURCES
    ${TRACY_SERVER_DIR}/TracyBroadcast.cpp
    ${TRACY_SERVER_DIR}/TracyMemory.cpp
    ${TRACY_SERVER_DIR}/TracyMmap.cpp
    ${TRACY_SERVER_DIR}/TracyPrint.cpp
    ${TRACY_SERVER_DIR}/TracySysUtil.cpp
    ${TRACY_SERVER_DIR}/TracyTaskDispatch.cpp
    ${TRACY_SERVER_DIR}/TracyTextureCompression.cpp
    ${TRACY_SERVER_DIR}/TracyThreadCompress.cpp
    ${TRACY_SERVER_DIR}/TracyWorker.cpp)

function(add_tracy_server target no_statistics)
    add_library(${target} STATIC EXCLUDE_FROM_ALL ${TRACY_COMMON_SOURCES} ${TRACY_SERVER_SOURCES})
    target_include_directories(${target} PUBLIC ${TRACY_COMMON_DIR} ${TRACY_SERVER_DIR})
    target_link_libraries(${target} PUBLIC TracyCapstone libzstd PPQSort::PPQSort)
    if(no_statistics)
        target_compile_definitions(${target} PUBLIC TRACY_NO_STATISTICS)
    endif()
endfunction()

add_tracy_server(TracyServerCapture ON)
add_tracy_server(TracyServerCsvexport OFF)

include(${TRACY_ROOT}/cmake/GitRef.cmake)

set(GETOPT_DIR "${TRACY_ROOT}/getopt")
add_library(TracyGetOpt STATIC EXCLUDE_FROM_ALL ${GETOPT_DIR}/getopt.c ${GETOPT_DIR}/getopt.h)
target_include_directories(TracyGetOpt PUBLIC ${GETOPT_DIR})

add_executable(tracy-capture
    ${TRACY_ROOT}/capture/src/capture.cpp
    ${TRACY_ROOT}/capture/src/CaptureOutput.cpp)
add_executable(tracy-csvexport
    ${TRACY_ROOT}/csvexport/src/csvexport.cpp)

target_include_directories(tracy-capture PRIVATE ${TRACY_ROOT}/capture/src)
target_include_directories(tracy-csvexport PRIVATE ${TRACY_ROOT}/csvexport/src)

target_link_libraries(tracy-capture PRIVATE TracyServerCapture TracyGetOpt)
target_link_libraries(tracy-csvexport PRIVATE TracyServerCsvexport TracyGetOpt)

add_git_ref(tracy-capture)
add_git_ref(tracy-csvexport)
