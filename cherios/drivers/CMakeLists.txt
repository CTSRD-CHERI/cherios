include_directories(include)

add_subdirectory(uart)

if("${HARDWARE}" STREQUAL "qemu")
    add_subdirectory(virtio)
elseif("${HARDWARE}" STREQUAL "fpga")
    add_subdirectory(alteraSD)
endif()
