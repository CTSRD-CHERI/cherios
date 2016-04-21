
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SOCKETS_SRCS
    src/init.S
    src/main.c
    src/sockets.c
)

add_cherios_executable(sockets LINKER_SCRIPT sandbox.ld SOURCES ${SOCKETS_SRCS})
