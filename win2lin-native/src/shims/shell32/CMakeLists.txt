add_library(win_shell32 SHARED
    shell32.cpp
)

target_link_libraries(win_shell32 PRIVATE win2lin_common)
target_include_directories(win_shell32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_shell32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_shell32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "shell32"
)
