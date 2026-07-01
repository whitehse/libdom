cmake_minimum_required(VERSION 3.14)
project(libdom VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Compiler warnings ─────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wno-unused-parameter
)

# ── Library ───────────────────────────────────────────────────────────
add_library(dom STATIC src/dom.c)
target_include_directories(dom PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ── Tests ─────────────────────────────────────────────────────────────
enable_testing()

add_executable(dom_smoke tests/test_smoke.c)
target_link_libraries(dom_smoke PRIVATE dom)
add_test(NAME dom_smoke COMMAND dom_smoke)

add_executable(dom_dialectic tests/test_dialectic.c)
target_link_libraries(dom_dialectic PRIVATE dom)
add_test(NAME dom_dialectic COMMAND dom_dialectic)

add_executable(dom_errors tests/test_errors.c)
target_link_libraries(dom_errors PRIVATE dom)
add_test(NAME dom_errors COMMAND dom_errors)

# ── Fuzz (optional, requires -fsanitize=fuzzer) ───────────────────────
option(DOM_FUZZ "Build fuzz harness (requires clang with libFuzzer)" OFF)
if(DOM_FUZZ)
    add_executable(dom_fuzz fuzz/fuzz_dom.c)
    target_link_libraries(dom_fuzz PRIVATE dom)
    target_compile_options(dom_fuzz PRIVATE -fsanitize=fuzzer,address)
    target_link_options(dom_fuzz PRIVATE -fsanitize=fuzzer,address)
endif()

# ── Example ───────────────────────────────────────────────────────────
add_executable(dom_example examples/example_basic.c)
target_link_libraries(dom_example PRIVATE dom)

# ── Valgrind targets ─────────────────────────────────────────────────
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(valgrind_smoke
        COMMAND ${VALGRIND} --leak-check=full --error-exitcode=1
                $<TARGET_FILE:dom_smoke>
        DEPENDS dom_smoke
        COMMENT "Running valgrind on dom_smoke"
    )
    add_custom_target(valgrind_dialectic
        COMMAND ${VALGRIND} --leak-check=full --error-exitcode=1
                $<TARGET_FILE:dom_dialectic>
        DEPENDS dom_dialectic
        COMMENT "Running valgrind on dom_dialectic"
    )
    add_custom_target(valgrind_errors
        COMMAND ${VALGRIND} --leak-check=full --error-exitcode=1
                $<TARGET_FILE:dom_errors>
        DEPENDS dom_errors
        COMMENT "Running valgrind on dom_errors"
    )
    add_custom_target(valgrind
        DEPENDS valgrind_smoke valgrind_dialectic valgrind_errors
    )
endif()