include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/main.c
    src/mmap.c
    src/release.c
)

add_cherios_memmgr(memmgt ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(memmgt PROPERTIES COMPILE_FLAGS -mxgot)
