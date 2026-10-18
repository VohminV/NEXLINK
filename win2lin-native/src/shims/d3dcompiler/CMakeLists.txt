add_library(win_d3dcompiler SHARED
    d3dcompiler.cpp
)

target_link_libraries(win_d3dcompiler PRIVATE win2lin_common)
target_include_directories(win_d3dcompiler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_d3dcompiler PRIVATE -fvisibility=default -O2)
set_target_properties(win_d3dcompiler PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "d3dcompiler"
)
