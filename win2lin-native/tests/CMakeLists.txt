# Test fixture: a tiny CRT-free Windows PE built with the MinGW cross compiler.
# If MinGW is unavailable the fixture is skipped and the exec-based checks are
# disabled, but the pure converter tests still run against a checked-in PE if
# one exists.
find_program(MINGW_CC x86_64-w64-mingw32-gcc)
find_program(MINGW_CXX x86_64-w64-mingw32-g++)

set(WIN2LIN_TEST_PE "")
set(WIN2LIN_GUI_TEST_PE "")

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

# Phase 1.5 fixture: a standard MSVCRT-linked Win32 + DXGI application.
# -static-libgcc -static-libstdc++ keeps the C++ runtime inside the PE so the
# only imports are msvcrt/user32/dxgi/kernel32 - exactly the shim surface.
if(MINGW_CXX)
    set(FIXTURE_GUI_PE "${CMAKE_CURRENT_BINARY_DIR}/hello_window.exe")
    add_custom_command(
        OUTPUT "${FIXTURE_GUI_PE}"
        COMMAND "${MINGW_CXX}" -O2 -static-libgcc -static-libstdc++
                -Wl,--dynamicbase
                -o "${FIXTURE_GUI_PE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/msvcrt_gui/hello_window.cpp"
                -luser32 -ldxgi
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/msvcrt_gui/hello_window.cpp"
        COMMENT "Building MSVCRT GUI PE test fixture (hello_window.exe)"
        VERBATIM
    )
    add_custom_target(win2lin_gui_test_fixture ALL DEPENDS "${FIXTURE_GUI_PE}")
    set(WIN2LIN_GUI_TEST_PE "${FIXTURE_GUI_PE}")
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

# ---------------------------------------------------------------------------
# Phase 1.5: MSVCRT + mock user32 + DXGI skeleton integration test
# ---------------------------------------------------------------------------
if(MINGW_CXX)
    add_executable(test_msvcrt_gui
        test_msvcrt_gui.cpp
    )

    target_link_libraries(test_msvcrt_gui
        PRIVATE
        win2lin_relinker
        ${LIEF_LIBRARIES}
    )

    target_include_directories(test_msvcrt_gui PRIVATE ${LIEF_INCLUDE_DIRS})

    target_compile_definitions(test_msvcrt_gui PRIVATE
        WIN2LIN_GUI_TEST_PE="${WIN2LIN_GUI_TEST_PE}"
        WIN2LIN_SHIM_DIR="$<TARGET_FILE_DIR:win_kernel32>"
        WIN2LIN_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_dependencies(test_msvcrt_gui win2lin_gui_test_fixture win_msvcrt win_user32 win_dxgi)

    add_test(NAME msvcrt_gui COMMAND test_msvcrt_gui)
endif()

if(MINGW_CC)
    set(FIXTURE_RELOC_PE "${CMAKE_CURRENT_BINARY_DIR}/reloc_test.exe")
    add_custom_command(
        OUTPUT "${FIXTURE_RELOC_PE}"
        COMMAND "${MINGW_CC}" -O2 -nostdlib
                -Wl,--dynamicbase -Wl,-e,EntryPoint
                -o "${FIXTURE_RELOC_PE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/reloc_test.c"
                -lkernel32
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/reloc_test.c"
        COMMENT "Building relocation test PE fixture (reloc_test.exe)"
        VERBATIM
    )
    add_custom_target(win2lin_reloc_fixture ALL DEPENDS "${FIXTURE_RELOC_PE}")
    set(WIN2LIN_RELOC_TEST_PE "${FIXTURE_RELOC_PE}")
endif()

# ---------------------------------------------------------------------------
# Phase 5: Base relocation test
# ---------------------------------------------------------------------------
add_executable(test_reloc
    test_reloc.cpp
)

target_link_libraries(test_reloc
    PRIVATE
    win2lin_relinker
    ${LIEF_LIBRARIES}
)

target_include_directories(test_reloc PRIVATE ${LIEF_INCLUDE_DIRS})

target_compile_definitions(test_reloc PRIVATE
    WIN2LIN_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

if(MINGW_CC)
    add_dependencies(test_reloc win2lin_reloc_fixture)
endif()

add_test(NAME reloc COMMAND test_reloc
    ${WIN2LIN_RELOC_TEST_PE}
    "$<TARGET_FILE_DIR:win_kernel32>"
    "${CMAKE_CURRENT_BINARY_DIR}/reloc_test.elf"
)
add_executable(test_d3d9
    test_d3d9.cpp
)

target_link_libraries(test_d3d9 PRIVATE win2lin_common dl)

target_include_directories(test_d3d9 PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME d3d9 COMMAND test_d3d9 WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
