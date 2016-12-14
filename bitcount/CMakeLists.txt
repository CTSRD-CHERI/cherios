
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/bitcnt_1.c
    src/bitcnt_2.c
    src/bitcnt_3.c
    src/bitcnt_4.c
    src/main.c
)

add_cherios_executable(bitcount ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(bitcount PROPERTIES COMPILE_FLAGS -mxgot)
