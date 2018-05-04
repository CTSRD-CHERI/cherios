include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(VIRTIO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/virtio.c
)

add_subdirectory(virtio-blk)
add_subdirectory(virtio-net)