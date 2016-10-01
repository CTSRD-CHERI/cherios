
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/loop.S
    src/main.c
)

add_cherios_executable(test2a ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(test2a PROPERTIES COMPILE_FLAGS -mxgot)
