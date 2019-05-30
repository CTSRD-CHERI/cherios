
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(PRGA_SRCS
    ${INIT_ASM}
    src/main.c
    src/sockets.c
)

add_cherios_executable(prga ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${PRGA_SRCS})
