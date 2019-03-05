include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    ${INIT_ASM}
    src/main.c
    src/mmap.c
    src/vmem.c
    src/pmem.c
    src/vmem_init.c
)

add_cherios_memmgr(memmgt ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})

