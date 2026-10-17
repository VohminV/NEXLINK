cmake_minimum_required(VERSION 3.24)
project(Win2LinNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position independent code for shared libs
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIEF REQUIRED LIEF)

find_package(Vulkan REQUIRED)

enable_testing()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIEF_INCLUDE_DIRS})

# Add subdirectories
add_subdirectory(src/relinker)
add_subdirectory(src/common)
add_subdirectory(src/shims/kernel32)
add_subdirectory(src/shims/user32)
add_subdirectory(src/shims/dxgi)
add_subdirectory(src/shims/d3d11)
add_subdirectory(src/upscaler)
add_subdirectory(tests)
