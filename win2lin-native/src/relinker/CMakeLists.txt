add_library(win2lin_relinker STATIC
    pe_to_elf.cpp
)

target_link_libraries(win2lin_relinker
    PUBLIC
    win2lin_common
    PRIVATE
    ${LIEF_LIBRARIES}
)

target_include_directories(win2lin_relinker
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${LIEF_INCLUDE_DIRS}
)

add_executable(win2lin-relinker
    main.cpp
)

target_link_libraries(win2lin-relinker
    PRIVATE
    win2lin_relinker
)
