cmake_minimum_required(VERSION 3.0)
project(qthu)

add_executable(qasm)
include(cmake/quickjs.cmake)
target_sources(qasm
    PRIVATE
    src/qasm.cpp
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