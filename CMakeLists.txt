cmake_minimum_required(VERSION 3.10)
project(qthu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# emulator of an assembler with qjs-like opcodes
add_executable(qasm)
include(cmake/quickjs.cmake)
target_sources(qasm
    PRIVATE
    src/asm/qasm.cpp
    src/asm/asm.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_link_libraries(qasm
    PRIVATE 
    quickjs
)

# qjs bytecode runner
add_executable(bcrun)
target_sources(bcrun
    PRIVATE
    src/bcrun.c
)

target_link_libraries(bcrun
    PRIVATE
    quickjs-libc
)

# compiler from cthu to qjs bytecode
add_executable(ct2qjs)
target_sources(ct2qjs
    PRIVATE
        src/ct2qjs/main.cpp
        src/ct2qjs/lexer.cpp
        src/ct2qjs/reader.cpp
        src/ct2qjs/codegen.cpp
    src/asm/asm.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_include_directories(ct2qjs
    PRIVATE
    ${CMAKE_SOURCE_DIR}/bricks
)

target_link_libraries(ct2qjs
    PRIVATE
    quickjs-libc
)

# compiler from javascript to cthu
add_executable(js2ct)

target_sources(js2ct
    PRIVATE
    src/js2ct/main.cpp
    src/js2ct/frontend/lexer.cpp
    src/js2ct/frontend/parser.cpp
    src/js2ct/printer/pretty_printer.cpp
)
