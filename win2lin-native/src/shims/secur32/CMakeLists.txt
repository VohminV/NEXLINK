add_library(win_secur32 SHARED
    secur32.cpp
)

target_link_libraries(win_secur32 PRIVATE win2lin_common)
target_include_directories(win_secur32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_secur32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_secur32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "secur32"
)
