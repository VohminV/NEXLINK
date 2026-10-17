add_library(win_dxgi SHARED
    dxgi.cpp
)

target_link_libraries(win_dxgi PRIVATE win2lin_common)
target_include_directories(win_dxgi PRIVATE /../../)
