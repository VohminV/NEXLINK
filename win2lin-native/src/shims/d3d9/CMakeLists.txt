add_library(win_d3d9 SHARED
    d3d9.cpp
)

target_link_libraries(win_d3d9 PRIVATE win2lin_common Vulkan::Vulkan SDL2::SDL2)
target_include_directories(win_d3d9 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_d3d9 PRIVATE -fvisibility=default -O2)
set_target_properties(win_d3d9 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "d3d9"
)
