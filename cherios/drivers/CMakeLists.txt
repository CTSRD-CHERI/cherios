include_directories(include)

add_subdirectory(uart)

if(NOT ${BAREBONES})
if("${HARDWARE}" STREQUAL "qemu")
    add_subdirectory(virtio)
elseif("${HARDWARE}" STREQUAL "fpga")
    add_subdirectory(alteraSD)
endif()
endif()
