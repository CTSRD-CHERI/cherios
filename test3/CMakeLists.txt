
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/fwd.S
    src/main.c
)

add_cherios_executable(test3 ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(test3 PROPERTIES COMPILE_FLAGS -mxgot)
