add_library(win_dwmapi SHARED
    dwmapi.cpp
)

target_link_libraries(win_dwmapi PRIVATE win2lin_common)
target_include_directories(win_dwmapi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_dwmapi PRIVATE -fvisibility=default -O2)
set_target_properties(win_dwmapi PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "dwmapi"
)
