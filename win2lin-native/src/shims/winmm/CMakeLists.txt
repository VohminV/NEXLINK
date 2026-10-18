add_library(win_winmm SHARED
    winmm.cpp
)

target_link_libraries(win_winmm PRIVATE win2lin_common)
target_include_directories(win_winmm PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_winmm PRIVATE -fvisibility=default -O2)
set_target_properties(win_winmm PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "winmm"
)
