add_library(win_user32 SHARED
    user32.cpp
)

target_link_libraries(win_user32 PRIVATE win2lin_common)
target_include_directories(win_user32 PRIVATE /../../)
