add_library(win_shaders STATIC
    dxbc_parser.cpp
    dxbc_to_spirv.cpp
    shader_cache.cpp
)

target_link_libraries(win_shaders PRIVATE Vulkan::Vulkan)
target_include_directories(win_shaders PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_shaders PRIVATE -O2 -Wall)
