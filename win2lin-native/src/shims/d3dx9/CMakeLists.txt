add_library(win_d3dx9 SHARED
    d3dx9.cpp
)

target_link_libraries(win_d3dx9 PRIVATE win2lin_common)
target_include_directories(win_d3dx9 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_d3dx9 PRIVATE -fvisibility=default -O2)
set_target_properties(win_d3dx9 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "d3dx9"
)
