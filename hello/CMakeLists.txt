
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(HELLO_SRCS
    ${INIT_ASM}
    src/main.c
)

add_cherios_executable(hello ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${HELLO_SRCS})
