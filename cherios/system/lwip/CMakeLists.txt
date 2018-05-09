get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${LWIP_INCLUDE})
include_directories(${DRIVERS_DIR}/virtio/include)
include_directories(${DRIVERS_DIR}/virtio/virtio-net/include)

set(X_SRCS
    ${INIT_ASM}
    ${lwipcore_SRCS}
    ${lwipcore4_SRCS}
    ${LWIP_DIR}/src/netif/ethernet.c
    ${LWIP_DIR}/src/netif/bridgeif.c
    ${LWIP_DIR}/src/netif/bridgeif_fdb.c
    ${LWIP_DIR}/src/apps/http/httpd.c
    ${LWIP_DIR}/src/apps/http/fs.c
    ${LWIP_DIR}/src/api/err.c
    src/sys_arch.c
    src/main.c
    src/httpd.c
    ${DRIVERS_DIR}/virtio/src/virtio.c

)

add_cherios_executable(${ACT_NAME} ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
