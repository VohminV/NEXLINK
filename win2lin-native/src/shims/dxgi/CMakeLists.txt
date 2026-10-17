add_library(win_dxgi SHARED
    dxgi.cpp
)

target_link_libraries(win_dxgi PRIVATE win2lin_common)
target_include_directories(win_dxgi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_dxgi PRIVATE -fvisibility=default -O2)
set_target_properties(win_dxgi PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "dxgi"
)
