add_library(win_ole32 SHARED
    ole32.cpp
)

target_link_libraries(win_ole32 PRIVATE win2lin_common)
target_include_directories(win_ole32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_ole32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_ole32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "ole32"
)
