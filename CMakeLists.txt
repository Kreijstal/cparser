cmake_minimum_required(VERSION 3.10)
project(ParserCalculator C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# Create a static library for the parser
add_library(parser_lib STATIC parser.c combinators.c)

# Add the executable for the standard calculator
add_executable(calculator calc.c)
target_link_libraries(calculator parser_lib)

# Add the executable for the tests
add_executable(tests tests.c)
target_link_libraries(tests parser_lib)
add_test(NAME tests COMMAND tests)

# Add the executable for the final calculator
# add_executable(calc calc.c)
# target_link_libraries(calc parser_lib)
