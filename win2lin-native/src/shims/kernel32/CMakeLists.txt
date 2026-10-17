add_library(win_kernel32 SHARED
    kernel32.cpp
)

target_link_libraries(win_kernel32 PRIVATE win2lin_common dl pthread)

target_include_directories(win_kernel32 PRIVATE ${CMAKE_SOURCE_DIR}/src)

# The shim exports Microsoft x64 ABI entry points; keep symbols visible and
# avoid over-optimisation that could merge/distort the ABI boundary.
target_compile_options(win_kernel32 PRIVATE -fvisibility=default -O2)

set_target_properties(win_kernel32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "kernel32"
)
