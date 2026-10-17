# Test fixture: a tiny CRT-free Windows PE built with the MinGW cross compiler.
# If MinGW is unavailable the fixture is skipped and the exec-based checks are
# disabled, but the pure converter tests still run against a checked-in PE if
# one exists.
find_program(MINGW_CC x86_64-w64-mingw32-gcc)

set(WIN2LIN_TEST_PE "")

if(MINGW_CC)
    set(FIXTURE_PE "${CMAKE_CURRENT_BINARY_DIR}/hello_win.exe")
    add_custom_command(
        OUTPUT "${FIXTURE_PE}"
        COMMAND "${MINGW_CC}" -O2 -nostdlib
                -Wl,--dynamicbase -Wl,-e,EntryPoint
                -o "${FIXTURE_PE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hello_win.c"
                -lkernel32
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hello_win.c"
        COMMENT "Building Windows PE test fixture (hello_win.exe)"
        VERBATIM
    )
    add_custom_target(win2lin_test_fixture ALL DEPENDS "${FIXTURE_PE}")
    set(WIN2LIN_TEST_PE "${FIXTURE_PE}")
endif()

add_executable(test_relinker
    test_relinker.cpp
)

target_link_libraries(test_relinker
    PRIVATE
    win2lin_relinker
    ${LIEF_LIBRARIES}
)

target_include_directories(test_relinker PRIVATE ${LIEF_INCLUDE_DIRS})

target_compile_definitions(test_relinker PRIVATE
    WIN2LIN_TEST_PE="${WIN2LIN_TEST_PE}"
    WIN2LIN_SHIM_DIR="$<TARGET_FILE_DIR:win_kernel32>"
    WIN2LIN_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

if(MINGW_CC)
    add_dependencies(test_relinker win2lin_test_fixture)
endif()

add_test(NAME relinker COMMAND test_relinker)
