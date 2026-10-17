add_library(win_user32 SHARED
    user32.cpp
)

target_link_libraries(win_user32 PRIVATE win2lin_common SDL2::SDL2)
target_include_directories(win_user32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_user32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_user32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "user32"
)
