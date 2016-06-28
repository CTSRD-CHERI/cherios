
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/heap.c
    src/main.c
    src/malloc.c
    src/mmap.c
    src/release.c
)

add_cherios_executable(memmgt ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
