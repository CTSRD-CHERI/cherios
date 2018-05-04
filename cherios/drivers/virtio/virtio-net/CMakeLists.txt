
include_directories(include)

set(VIRTIONET_SRCS
    ${INIT_ASM}
    ${VIRTIO_SRCS}
    src/main.c
    src/virtio_net.c
)

add_cherios_executable(virtio-net ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${VIRTIONET_SRCS})
set_target_properties(virtio-net PROPERTIES COMPILE_FLAGS -mxgot)
target_link_libraries(virtio-net CheriOS::LibUser)
