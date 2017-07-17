
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    ${INIT_ASM}
    src/loop.S
    src/main.c
)

add_cherios_executable(test2b ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(test2b PROPERTIES COMPILE_FLAGS -mxgot)
