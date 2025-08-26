cmake_minimum_required(VERSION 3.10)
project(ParserCalculator C)

# Create a static library for the parser
add_library(parser_lib STATIC parser.c combinators.c)

# Add the executable for the standard calculator
add_executable(calculator calc.c)
target_link_libraries(calculator parser_lib)

