get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${LWIP_INCLUDE})
include_directories(${DRIVERS_DIR}/virtio/include)
include_directories(${DRIVERS_DIR}/virtio/virtio-net/include)
include_directories(${DRIVERS_DIR}/megacore/include)

if("${HARDWARE}" STREQUAL "qemu")
    set(DRIVER_SRCS src/virtio/virtio_driver.c)
elseif("${HARDWARE}" STREQUAL "fpga")
    if("${MSGDMA}")
        set(DRIVER_SRCS src/megacore/megacore_driver.c src/megacore/altera_sgdma.c)
    else()
        set(DRIVER_SRCS src/megacore/megacore_driver.c src/megacore/altera_fifo.c)
    endif()
endif()


set(X_SRCS
    ${DEFAULT_INSECURE_ASM}
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
    src/checksum.S
    ${DRIVERS_DIR}/virtio/src/virtio.c
    ${DRIVER_SRCS}
)

# libuser now provides this
list(REMOVE_ITEM X_SRCS ${LWIP_DIR}/src/core/ipv4/ip4_addr.c)

add_cherios_executable(${ACT_NAME} ADD_TO_FILESYSTEM CAN_VARY_SS LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})

if("${HARDWARE}" STREQUAL "fpga")
    if("${MSGDMA}")
    else()
        target_compile_definitions(${ACT_NAME} PUBLIC -DDRIVER_ASM="megacore/fifo_asm.S")
    endif()
endif()