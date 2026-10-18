add_library(win_advapi32 SHARED
    advapi32.cpp
)

target_link_libraries(win_advapi32 PRIVATE win2lin_common)
target_include_directories(win_advapi32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_advapi32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_advapi32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "advapi32"
)
