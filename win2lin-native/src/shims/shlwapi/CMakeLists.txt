add_library(win_shlwapi SHARED
    shlwapi.cpp
)

target_link_libraries(win_shlwapi PRIVATE win2lin_common)
target_include_directories(win_shlwapi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_shlwapi PRIVATE -fvisibility=default -O2)
set_target_properties(win_shlwapi PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "shlwapi"
)
