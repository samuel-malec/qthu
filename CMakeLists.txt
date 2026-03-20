cmake_minimum_required(VERSION 3.10)
project(qthu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(bcrun)
target_sources(bcrun
    PRIVATE
    src/bcrun.c
)

target_link_libraries(bcrun
    PRIVATE
    quickjs-libc
)

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
