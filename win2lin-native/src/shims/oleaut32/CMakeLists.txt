add_library(win_oleaut32 SHARED
    oleaut32.cpp
)

target_link_libraries(win_oleaut32 PRIVATE win2lin_common)
target_include_directories(win_oleaut32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_oleaut32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_oleaut32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "oleaut32"
)
