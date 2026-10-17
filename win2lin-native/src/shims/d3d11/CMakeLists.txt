add_library(win_d3d11 SHARED
    d3d11.cpp
)

target_link_libraries(win_d3d11 PRIVATE win2lin_common)
target_include_directories(win_d3d11 PRIVATE /../../)
