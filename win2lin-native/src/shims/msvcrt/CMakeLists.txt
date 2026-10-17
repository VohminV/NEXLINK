add_library(win_msvcrt SHARED
    msvcrt.cpp
)

target_link_libraries(win_msvcrt PRIVATE win2lin_common dl pthread)
target_include_directories(win_msvcrt PRIVATE ${CMAKE_SOURCE_DIR}/src)
# The shim exports Microsoft x64 ABI entry points; keep symbols visible and
# avoid over-optimisation that could merge/distort the ABI boundary.
target_compile_options(win_msvcrt PRIVATE -fvisibility=default -O2)
# Version every export (see msvcrt.map): keeps the PE's unversioned imports
# bound to this shim while GLIBC-versioned references from libstdc++/other
# shims skip straight to real glibc.
target_link_options(win_msvcrt PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/msvcrt.map
)
set_target_properties(win_msvcrt PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "msvcrt"
)
