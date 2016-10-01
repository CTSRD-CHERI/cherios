
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(VIRTIOBLK_SRCS
    src/init.S
    src/main.c
    src/virtio_blk.c
)

add_cherios_executable(virtio-blk ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${VIRTIOBLK_SRCS})
set_target_properties(virtio-blk PROPERTIES COMPILE_FLAGS -mxgot)
target_link_libraries(virtio-blk CheriOS::LibUser)
