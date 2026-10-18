cmake_minimum_required(VERSION 3.24)

add_executable(test_dxbc_shader
    test_dxbc_shader.cpp
)

target_link_libraries(test_dxbc_shader PRIVATE win_shaders)

target_include_directories(test_dxbc_shader PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_options(test_dxbc_shader PRIVATE -O2 -Wall)

add_test(NAME dxbc_shader COMMAND test_dxbc_shader)
