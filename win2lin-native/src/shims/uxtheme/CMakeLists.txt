add_library(win_uxtheme SHARED
    uxtheme.cpp
)

target_link_libraries(win_uxtheme PRIVATE win2lin_common)
target_include_directories(win_uxtheme PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_uxtheme PRIVATE -fvisibility=default -O2)
set_target_properties(win_uxtheme PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "uxtheme"
)
