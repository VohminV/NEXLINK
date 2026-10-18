add_executable(win2lin-gui
    win2lin-gui.cpp
)

target_link_libraries(win2lin-gui PRIVATE SDL2::SDL2)
target_include_directories(win2lin-gui PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win2lin-gui PRIVATE -O2 -Wall)
set_target_properties(win2lin-gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
