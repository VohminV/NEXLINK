add_library(win_msvcp71 SHARED
    msvcp.cpp
)

target_link_libraries(win_msvcp71 PRIVATE win2lin_common)
target_include_directories(win_msvcp71 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_msvcp71 PRIVATE -fvisibility=default -O2)
set_target_properties(win_msvcp71 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "msvcp71"
)
