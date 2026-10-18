add_library(win_msvcp80 SHARED
    msvcp.cpp
)

target_link_libraries(win_msvcp80 PRIVATE win2lin_common)
target_include_directories(win_msvcp80 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_msvcp80 PRIVATE -fvisibility=default -O2)
set_target_properties(win_msvcp80 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "msvcp80"
)
