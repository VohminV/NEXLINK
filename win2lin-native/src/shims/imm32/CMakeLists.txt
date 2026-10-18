add_library(win_imm32 SHARED
    imm32.cpp
)

target_link_libraries(win_imm32 PRIVATE win2lin_common)
target_include_directories(win_imm32 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_imm32 PRIVATE -fvisibility=default -O2)
set_target_properties(win_imm32 PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "imm32"
)
