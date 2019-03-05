
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(VIRTIOBLK_SRCS
    ${INIT_ASM}
    ${VIRTIO_SRCS}
    src/main.c
    src/virtio_blk.c
)

add_cherios_executable(virtio-blk ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${VIRTIOBLK_SRCS})
