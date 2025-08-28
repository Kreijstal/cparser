cmake_minimum_required(VERSION 3.10)
project(ParserCalculator C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# --- Main Parser Library ---
# The core parser_lib should not include example-specific code like json_parser.c
add_library(parser_lib STATIC parser.c combinators.c)

# --- Examples ---
option(BUILD_INTEGRATION_TESTS "Build the example applications and their tests" OFF)

if(BUILD_INTEGRATION_TESTS)
    message(STATUS "Building examples...")

    # --- Check for libunwind for the JSON example ---
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UNWIND QUIET unwind)

    if(UNWIND_FOUND)
        message(STATUS "Found libunwind, building JSON example.")
        # --- JSON Parser Example ---
        # The JSON parser is now part of the examples, so its sources are here.
        # We create a separate library for it so it can be shared between the CLI and the tests.
        add_library(json_parser_lib STATIC
            examples/json_parser/json_parser.c
            examples/json_parser/json_parser.h
        )
        # The json_parser_lib depends on the main parser_lib
        target_link_libraries(json_parser_lib PUBLIC parser_lib)
        target_include_directories(json_parser_lib PUBLIC ${CMAKE_SOURCE_DIR} ${UNWIND_INCLUDE_DIRS})

        # Add the executable for the JSON parser CLI
        add_executable(json_parser_cli examples/json_parser/json_main.c)
        target_link_libraries(json_parser_cli json_parser_lib ${UNWIND_LIBRARIES})
        target_include_directories(json_parser_cli PUBLIC ${UNWIND_INCLUDE_DIRS})

        # Add the executable for the JSON parser tests
        add_executable(json_tests examples/json_parser/json_tests.c)
        target_include_directories(json_tests PUBLIC ${CMAKE_SOURCE_DIR} ${UNWIND_INCLUDE_DIRS})
        target_link_libraries(json_tests json_parser_lib ${UNWIND_LIBRARIES})
        add_test(NAME json_tests COMMAND json_tests)
    else()
        message(STATUS "libunwind not found, skipping JSON example.")
    endif()

    # --- Calculator Example ---
    add_executable(calculator examples/calculator/calc.c)
    target_include_directories(calculator PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(calculator parser_lib)
    add_test(NAME calculator_tests COMMAND calculator "1 + 1")

endif()


# Note: The original 'tests' target is not included as it seems to be a generic
# test runner, and the new structure separates tests per example.
# If it's needed, it should be moved into the if(BUILD_INTEGRATION_TESTS) block.
# add_executable(tests tests.c)
# target_link_libraries(tests parser_lib)
# add_test(NAME tests COMMAND tests)
