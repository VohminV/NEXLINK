add_library(win_d3d11 SHARED
    d3d11.cpp
    vulkan_backend.cpp
)

target_link_libraries(win_d3d11 PRIVATE win2lin_common Vulkan::Vulkan SDL2::SDL2 win_shaders)
target_include_directories(win_d3d11 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_d3d11 PRIVATE -fvisibility=default -O2)
set_target_properties(win_d3d11 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "d3d11"
)
