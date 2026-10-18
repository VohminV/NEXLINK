add_library(win_crypt32 SHARED
    crypt32.cpp
)

target_link_libraries(win_crypt32 PRIVATE win2lin_common)
target_include_directories(win_crypt32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_crypt32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_crypt32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "crypt32"
)
