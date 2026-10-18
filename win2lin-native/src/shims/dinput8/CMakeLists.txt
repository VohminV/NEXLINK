add_library(win_dinput8 SHARED
    dinput8.cpp
)

target_link_libraries(win_dinput8 PRIVATE win2lin_common)
target_include_directories(win_dinput8 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_dinput8 PRIVATE -fvisibility=default -O2)
set_target_properties(win_dinput8 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "dinput8"
)
