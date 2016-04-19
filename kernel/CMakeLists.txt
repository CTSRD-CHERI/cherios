set(CMAKE_C_COMPILER_TARGET cheri-unknown-freebsd)
set(ASM_COMPILER_FLAGS
    -mcpu=mips4
    -mabi=sandbox
    -cheri-linker
    -integrated-as
    -msoft-float
    # "-B${CheriSDK_TOOLCHAIN_DIR}"  # Doesn't seem to be required
)
set(C_COMPILER_FLAGS
    ${ASM_COMPILER_FLAGS}
    -nostdinc
    -O2
    -G0
    -mxgot
    -Werror
)
# Use strange CMake generator expressions to set per-language compile flags
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_COMPILER_FLAGS}>")

# TODO: only add -g when CMAKE_BUILD_TYPE == Debug
# for now we always want it
add_compile_options(-g)

include_directories(include)

set(LIBKERNEL_ASM_SRCS
    src/exception.S
    src/fs.S
    src/init.S
)
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

# add_library(libkernel STATIC ${LIBKERNEL_SRCS})
# # CMake prepends "lib" to library names but in this case we
# # don't want that as we add an executable called kernel
# # XXX: we could also call the executable cherios_kernel or similar
# set_target_properties(libkernel PROPERTIES PREFIX "")

add_executable(kernel
    ${LIBKERNEL_SRCS}
    ${LIBKERNEL_ASM_SRCS}
    ${KERNEL_SRCS}
    ${KERNEL_ASM_SRCS}
)
set(KERNEL_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ldscripts/kernel.ld")
target_link_libraries(kernel
    -cheri-linker
    "-L${CMAKE_SOURCE_DIR}/ldscripts" # to find the included mips.ld linker script
    -Wl "-T${KERNEL_LINKER_SCRIPT}"
    -G0
    -mxgot
    -nostartfiles
    -nodefaultlibs
)
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${KERNEL_LINKER_SCRIPT})
add_subdirectory(../libuser ${CMAKE_CURRENT_BINARY_DIR}/../libuser) # HACK: build libuser with the right compiler flags
target_link_libraries(kernel user)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel.dump
    DEPENDS kernel
    COMMAND "${CheriSDK_TOOLCHAIN_DIR}/objdump" -x -d -S "$<TARGET_FILE:kernel>" > "${CMAKE_CURRENT_BINARY_DIR}/kernel.dump"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(kernel_dump ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.dump)


