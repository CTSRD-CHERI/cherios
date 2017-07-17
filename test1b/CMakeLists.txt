
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    ${INIT_ASM}
    src/main.c
)

add_cherios_executable(test1b ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(test1b PROPERTIES COMPILE_FLAGS -mxgot)
