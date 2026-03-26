cmake_minimum_required(VERSION 3.10)
project(qthu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# toy assembler
add_executable(qasm)
include(cmake/quickjs.cmake)
target_sources(qasm
    PRIVATE
    src/qasm.cpp
    src/asm/qassembly.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_link_libraries(qasm
    PRIVATE 
    quickjs
)

# wrap the bytecode and run it
add_executable(bcrun)
target_sources(bcrun
    PRIVATE
    src/bcrun.c
)

target_link_libraries(bcrun
    PRIVATE
    quickjs-libc
)

# compiler for a toy language
add_executable(jsc)
target_sources(jsc
    PRIVATE
    src/jsc/jsc.cpp
    src/asm/qassembly.cpp
    src/asm/instruction.cpp
    src/asm/directive.cpp
)

target_link_libraries(jsc
    PRIVATE
    quickjs-libc
)

add_executable(qthucc)
target_sources(qthucc
    PRIVATE
    src/cthu/main.cpp
    src/cthu/lexer.cpp
    src/cthu/reader.cpp
)

target_include_directories(qthucc
    PRIVATE
    ${CMAKE_SOURCE_DIR}/bricks
)