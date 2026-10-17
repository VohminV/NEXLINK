add_library(upscaler SHARED
    upscaler.cpp
)

target_link_libraries(upscaler PRIVATE win2lin_common)
target_include_directories(upscaler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
