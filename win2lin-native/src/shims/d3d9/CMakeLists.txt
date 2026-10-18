add_library(win_d3d9 SHARED
    d3d9.cpp
    gen_shaders.py
)

find_program(GLSLANGVALIDATOR glslangValidator)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(GLSLANGVALIDATOR AND Python3_EXECUTABLE)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/simple.vert.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADER_DIR}/simple.vert -o ${CMAKE_CURRENT_BINARY_DIR}/simple.vert.spv
        DEPENDS ${SHADER_DIR}/simple.vert
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/simple.frag.spv
        COMMAND ${GLSLANGVALIDATOR} -V ${SHADER_DIR}/simple.frag -o ${CMAKE_CURRENT_BINARY_DIR}/simple.frag.spv
        DEPENDS ${SHADER_DIR}/simple.frag
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders_embedded.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_shaders.py
            ${CMAKE_CURRENT_BINARY_DIR}/shaders_embedded.h
            ${CMAKE_CURRENT_BINARY_DIR}/simple.vert.spv g_vert_spv
            ${CMAKE_CURRENT_BINARY_DIR}/simple.frag.spv g_frag_spv
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/simple.vert.spv
                ${CMAKE_CURRENT_BINARY_DIR}/simple.frag.spv
                ${CMAKE_CURRENT_SOURCE_DIR}/gen_shaders.py
        VERBATIM
    )

    target_sources(win_d3d9 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/shaders_embedded.h)

    target_compile_definitions(win_d3d9 PRIVATE WIN2LIN_D3D9_HAS_SHADERS)
    set(WIN2LIN_D3D9_HAS_SHADERS 1)
else()
    target_compile_definitions(win_d3d9 PRIVATE WIN2LIN_D3D9_NO_SHADERS)
    set(WIN2LIN_D3D9_HAS_SHADERS 0)
endif()

target_link_libraries(win_d3d9 PRIVATE win2lin_common Vulkan::Vulkan SDL2::SDL2)
target_include_directories(win_d3d9 PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(win_d3d9 PRIVATE -fvisibility=default -O2)
set_target_properties(win_d3d9 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "d3d9"
)
