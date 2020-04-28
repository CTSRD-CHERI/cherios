
include_directories(include)

set(VIRTIONET_SRCS
    ${INIT_ASM}
    ${VIRTIO_SRCS}
    src/main.c
    src/virtio_net.c
)

add_cherios_executable(virtio_net ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${VIRTIONET_SRCS})
