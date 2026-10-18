add_library(win_iphlpapi SHARED
    iphlpapi.cpp
)

target_link_libraries(win_iphlpapi PRIVATE win2lin_common)
target_include_directories(win_iphlpapi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(win_iphlpapi PRIVATE -fvisibility=default -O2)
set_target_properties(win_iphlpapi PROPERTIES
    PREFIX "libwin_"
    OUTPUT_NAME "iphlpapi"
)
