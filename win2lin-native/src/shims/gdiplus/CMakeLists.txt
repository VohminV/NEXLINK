add_library(win_gdiplus SHARED
    gdiplus.cpp
)

target_link_libraries(win_gdiplus PRIVATE win2lin_common)
target_include_directories(win_gdiplus PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_gdiplus PRIVATE -fvisibility=default -O2)
set_target_properties(win_gdiplus PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "gdiplus"
)
