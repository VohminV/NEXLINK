add_library(win_dsound SHARED
    dsound.cpp
)

target_link_libraries(win_dsound PRIVATE win2lin_common)
target_include_directories(win_dsound PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_dsound PRIVATE -fvisibility=default -O2)
set_target_properties(win_dsound PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "dsound"
)
