include_directories(include)

set(LIBKERNEL_ASM_SRCS
    src/exception.S
    src/fs.S
    src/init.S
)

# fs.S depends on the fs.img file
set_source_files_properties(src/exception.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fs.img"
)
# we have to include ${CMAKE_CURRENT_BINARY_DIR} for all files and not just fs.c
# because CMake strips include paths from the COMPILE_FLAGS source file property
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # to find fs.img
# set_source_files_properties(src/exception.S PROPERTIES
#     COMPILE_FLAGS "-I${CMAKE_CURRENT_BINARY_DIR}/foo"
# )

set(LIBKERNEL_SRCS
    src/ccall.c
    src/cp0.c
    src/elf_loader.c
    src/heap.c
    src/kernel_exceptions.c
    src/kernel_printf.c
    src/kernel_utils.c
    src/malloc.c
    src/syscalls.c
    src/timer.c
    src/uart.c
    src/ufs_module.c
    src/ufs_read.c
)
set(KERNEL_SRCS
    src/main.c
)
set(KERNEL_ASM_SRCS
)

set(LINKKERNEL_SRCS
    src/crtbeginC.c
    src/crtendC.c
)

#
# Configure a console driver at compile time.  Default to the UART found in
# the MALTA reference board ("malta").  Other options are "altera" for the
# ALTERA JTAG UART used for BERI on FPGA, and "gxemul" for the GXemul
# low-level console device.
#
set(CONSOLE "malta" CACHE STRING 
"The console driver to use. Defaults to the UART found in the MALTA reference \
board (\"malta\").  Other options are \"altera\" for the ALTERA JTAG UART \
used for BERI on FPGA, and \"gxemul\" for the GXemul low-level console device.")
if("${CONSOLE}" STREQUAL "malta")
    list(APPEND LIBKERNEL_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND LIBKERNEL_SRCS src/uart_altera.c)
elseif("${CONSOLE}" STREQUAL "gxemul")
    list(APPEND LIBKERNEL_SRCS src/uart_gxemul.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_executable(cherios LINKER_SCRIPT kernel.ld
    ${LIBKERNEL_SRCS}
    ${LIBKERNEL_ASM_SRCS}
    ${KERNEL_SRCS}
    ${KERNEL_ASM_SRCS}
)
target_link_libraries(cherios CheriOS::LibUser)


# TODO: actually create fs.img
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.img
    COMMAND echo 123456 > "${CMAKE_CURRENT_BINARY_DIR}/fs.img"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
