include(FetchContent)

FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/bellard/quickjs.git
    GIT_TAG master
)

FetchContent_MakeAvailable(quickjs)

add_library(quickjs STATIC
    ${quickjs_SOURCE_DIR}/quickjs.c
    ${quickjs_SOURCE_DIR}/libregexp.c
    ${quickjs_SOURCE_DIR}/libunicode.c
    ${quickjs_SOURCE_DIR}/cutils.c
    ${quickjs_SOURCE_DIR}/dtoa.c
)

target_include_directories(quickjs PUBLIC
    ${quickjs_SOURCE_DIR}
)

target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION=\"2025-09-13\"
    HAVE_CLOSEFROM
)

target_link_libraries(quickjs PRIVATE m)

add_library(quickjs-libc STATIC
    ${quickjs_SOURCE_DIR}/quickjs-libc.c
)

target_include_directories(quickjs-libc PUBLIC
    ${quickjs_SOURCE_DIR}
)

target_compile_definitions(quickjs-libc PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION=\"2025-09-13\"
    HAVE_CLOSEFROM
)

target_link_libraries(quickjs-libc PUBLIC quickjs m dl pthread)

add_executable(qjsc
    ${quickjs_SOURCE_DIR}/qjsc.c
)

target_link_libraries(qjsc PRIVATE quickjs-libc)

target_compile_definitions(qjsc PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION=\"2025-09-13\"
    HAVE_CLOSEFROM
)
