add_library(win2lin_common STATIC
    logging.cpp
    utils.cpp
    abi_defs.cpp
)

target_include_directories(win2lin_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
