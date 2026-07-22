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

# compiler from a toy language to qjs bytecode
add_executable(toy)
target_sources(toy
    PRIVATE
    src/toy/toy.cpp
    src/asm/asm.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_link_libraries(toy
    PRIVATE
    quickjs-libc
)

# compiler from cthu to qjs bytecode
add_executable(cthuc)
target_sources(cthuc
    PRIVATE
    src/cthuc/main.cpp
    src/cthuc/lexer.cpp
    src/cthuc/reader.cpp
    src/cthuc/codegen.cpp
    src/asm/asm.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_include_directories(cthuc
    PRIVATE
    ${CMAKE_SOURCE_DIR}/bricks
)

target_link_libraries(cthuc
    PRIVATE
    quickjs-libc
)

# compiler from javascript to cthu
add_executable(jsc)

target_sources(jsc
    PRIVATE
    src/jsc/main.cpp
    src/jsc/frontend/lexer.cpp
    src/jsc/frontend/parser.cpp
    src/jsc/printer/pretty_printer.cpp
)
