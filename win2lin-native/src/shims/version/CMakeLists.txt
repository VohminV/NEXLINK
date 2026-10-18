add_library(win_version SHARED
    version.cpp
)

target_link_libraries(win_version PRIVATE win2lin_common)
target_include_directories(win_version PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_version PRIVATE -fvisibility=default -O2)
set_target_properties(win_version PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "version"
)
