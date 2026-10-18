cmake_minimum_required(VERSION 3.24)
project(Win2LinNative VERSION 0.4.0 LANGUAGES CXX)

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

find_package(SDL2 REQUIRED)
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
add_subdirectory(src/shims/msvcrt)
add_subdirectory(src/shims/user32)
add_subdirectory(src/shims/dxgi)
add_subdirectory(src/shims/advapi32)
add_subdirectory(src/shims/ws2_32)
add_subdirectory(src/shims/winmm)
add_subdirectory(src/shims/dinput8)
add_subdirectory(src/shims/dsound)
add_subdirectory(src/shims/version)
add_subdirectory(src/shims/shell32)
add_subdirectory(src/shims/shlwapi)
add_subdirectory(src/shims/ole32)
add_subdirectory(src/shims/oleaut32)
add_subdirectory(src/shims/d3d9)
add_subdirectory(src/shims/d3dx9)
add_subdirectory(src/shims/d3dcompiler)
add_subdirectory(src/shims/imm32)
add_subdirectory(src/shims/usp10)
add_subdirectory(src/shims/uxtheme)
add_subdirectory(src/shims/dwmapi)
add_subdirectory(src/shims/iphlpapi)
add_subdirectory(src/shims/crypt32)
add_subdirectory(src/shims/secur32)
add_subdirectory(src/shims/gdiplus)
add_subdirectory(src/shims/msvcp71)
add_subdirectory(src/shims/msvcp80)
add_subdirectory(src/shims/msvcp90)
add_subdirectory(src/shims/msacm32)
add_subdirectory(src/gui)
add_subdirectory(tests)
