cmake_minimum_required(VERSION 3.10)
project(ParserCalculator C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# --- Main Parser Library ---
add_library(parser_lib STATIC parser.c combinators.c)

# --- Unit Tests ---
# These are the core unit tests for the parser library. They are always built.
add_executable(tests tests.c)
target_link_libraries(tests parser_lib)
add_test(unit_tests tests)


# --- Examples and Integration Tests ---
option(BUILD_INTEGRATION_TESTS "Build the example applications and their tests" OFF)

if(BUILD_INTEGRATION_TESTS)
    message(STATUS "Building integration tests and examples...")

    # --- Calculator Example ---
    add_executable(calculator examples/calculator/calc.c)
    target_include_directories(calculator PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(calculator parser_lib)
    add_test(NAME calculator_tests COMMAND ${CMAKE_SOURCE_DIR}/examples/calculator/test_calculator.sh)

    # --- JSON Parser Example (optional, depends on libunwind) ---
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(UNWIND QUIET libunwind)
        if(UNWIND_FOUND)
            message(STATUS "Found libunwind, building JSON example.")
            add_library(json_parser_lib STATIC
                examples/json_parser/json_parser.c
                examples/json_parser/json_parser.h
            )
            target_link_libraries(json_parser_lib PUBLIC parser_lib)
            target_include_directories(json_parser_lib PUBLIC ${CMAKE_SOURCE_DIR} ${UNWIND_INCLUDE_DIRS})

            add_executable(json_parser_cli examples/json_parser/json_main.c)
            target_link_libraries(json_parser_cli json_parser_lib ${UNWIND_LIBRARIES})
            target_include_directories(json_parser_cli PUBLIC ${UNWIND_INCLUDE_DIRS})

            add_executable(json_tests examples/json_parser/json_tests.c)
            target_include_directories(json_tests PUBLIC ${CMAKE_SOURCE_DIR} ${UNWIND_INCLUDE_DIRS})
            target_link_libraries(json_tests json_parser_lib ${UNWIND_LIBRARIES})
            add_test(NAME json_tests COMMAND json_tests --verbose=3)
        else()
            message(STATUS "libunwind not found, skipping JSON example.")
        endif()
    else()
        message(STATUS "pkg-config not found, skipping JSON example.")
    endif()

    # --- FPC Parser Example (Disabled) ---
    # add_library(fpc_parser_lib STATIC
    #     examples/fpc_parser/fpc_parser.c
    #     examples/fpc_parser/fpc_parser.h
    # )
    # target_link_libraries(fpc_parser_lib PUBLIC parser_lib)
    # target_include_directories(fpc_parser_lib PUBLIC ${CMAKE_SOURCE_DIR})
    #
    # add_executable(fpc_tests examples/fpc_parser/fpc_tests.c)
    # target_include_directories(fpc_tests PUBLIC ${CMAKE_SOURCE_DIR})
    # target_link_libraries(fpc_tests fpc_parser_lib)
    # add_test(NAME fpc_tests COMMAND fpc_tests)

    # --- Pascal Parser Example ---
    add_library(pascal_parser_lib STATIC
        examples/pascal_parser/pascal_parser.c
        examples/pascal_parser/pascal_parser.h
    )
    target_link_libraries(pascal_parser_lib PUBLIC parser_lib)
    target_include_directories(pascal_parser_lib PUBLIC ${CMAKE_SOURCE_DIR})

    add_executable(pascal_tests examples/pascal_parser/pascal_tests.c)
    target_include_directories(pascal_tests PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(pascal_tests pascal_parser_lib)
    add_test(NAME pascal_tests COMMAND pascal_tests)

    add_executable(pascal_driver examples/pascal_parser/pascal_driver.c)
    target_include_directories(pascal_driver PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(pascal_driver pascal_parser_lib)

endif()

# --- Debug Program ---
add_executable(debug_pascal debug_pascal.c)
target_include_directories(debug_pascal PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(debug_pascal parser_lib pascal_parser_lib)

# --- Simple Debug Program ---
add_executable(simple_debug simple_debug.c)
target_include_directories(simple_debug PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(simple_debug parser_lib)
