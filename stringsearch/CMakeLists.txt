
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/bmhasrch.c
    src/bmhisrch.c
    src/bmhsrch.c
    src/main.c
)

add_cherios_executable(stringsearch ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(stringsearch PROPERTIES COMPILE_FLAGS -mxgot)
