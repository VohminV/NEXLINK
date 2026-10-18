add_library(win_msacm32 SHARED
    msacm32.cpp
)

target_link_libraries(win_msacm32 PRIVATE win2lin_common)
target_include_directories(win_msacm32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_msacm32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_msacm32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "msacm32"
)
