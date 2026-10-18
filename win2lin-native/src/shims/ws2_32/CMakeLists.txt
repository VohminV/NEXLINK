add_library(win_ws2_32 SHARED
    ws2_32.cpp
)

target_link_libraries(win_ws2_32 PRIVATE win2lin_common)
target_include_directories(win_ws2_32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_ws2_32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_ws2_32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "ws2_32"
)
