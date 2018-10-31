include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/elf/include)
include_directories(${CMAKE_SOURCE_DIR}/tmpalloc/include)

set(PROC_SRCS
    ${INIT_ASM}
    src/main.c
    ${CMAKE_SOURCE_DIR}/elf/src/elf_loader.c
    ${CMAKE_SOURCE_DIR}/elf/src/cprogram.c
    ${CMAKE_SOURCE_DIR}/tmpalloc/src/alloc.c
)

add_cherios_executable_early(proc ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${PROC_SRCS})
