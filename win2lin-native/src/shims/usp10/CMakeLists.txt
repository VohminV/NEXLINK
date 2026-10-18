add_library(win_usp10 SHARED
    usp10.cpp
)

target_link_libraries(win_usp10 PRIVATE win2lin_common)
target_include_directories(win_usp10 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_usp10 PRIVATE -fvisibility=default -O2)
set_target_properties(win_usp10 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "usp10"
)
