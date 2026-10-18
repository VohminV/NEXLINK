add_library(win_msvcp90 SHARED
    msvcp.cpp
)

target_link_libraries(win_msvcp90 PRIVATE win2lin_common)
target_include_directories(win_msvcp90 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_msvcp90 PRIVATE -fvisibility=default -O2)
set_target_properties(win_msvcp90 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "msvcp90"
)
