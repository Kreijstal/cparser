cmake_minimum_required(VERSION 3.10)
project(ParserCalculator C)

# Create a static library for the parser
add_library(parser_lib STATIC parser.c)

# Add the executable for the standard calculator
add_executable(calculator calc.c)
target_link_libraries(calculator parser_lib)

# Add the executable for the 'until' test
add_executable(test_until test_until.c)
target_link_libraries(test_until parser_lib)
