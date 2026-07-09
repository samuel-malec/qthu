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
add_executable(cthucc)
target_sources(cthucc
    PRIVATE
    src/cthucc/main.cpp
    src/cthucc/lexer.cpp
    src/cthucc/reader.cpp
    src/cthucc/codegen.cpp
    src/asm/asm.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_include_directories(cthucc
    PRIVATE
    ${CMAKE_SOURCE_DIR}/bricks
)

target_link_libraries(cthucc
    PRIVATE
    quickjs-libc
)

# compiler from javascript to cthu
add_executable(jscc)

target_sources(jscc
    PRIVATE
    src/jscc/main.cpp
    src/jscc/frontend/lexer.cpp
    src/jscc/frontend/parser.cpp
)
