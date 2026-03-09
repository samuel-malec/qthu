cmake_minimum_required(VERSION 3.0)
project(qthu)

add_executable(main)
include(cmake/quickjs.cmake)
target_sources(main
    PRIVATE
    src/main.cpp
    src/qasm.cpp
)

target_link_libraries(main 
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