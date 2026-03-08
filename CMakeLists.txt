cmake_minimum_required(VERSION 3.0)
project(qthu)

add_executable(qthu)
include(cmake/quickjs.cmake)
target_sources(qthu
    PRIVATE
    src/qasm.cpp
)

target_link_libraries(qthu 
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
    quickjs
)