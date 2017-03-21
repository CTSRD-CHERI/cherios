include_directories(include)

# fs.S depends on the fs.img file
set_source_files_properties(src/boot/fs.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fs.img"
)
# we have to include ${CMAKE_CURRENT_BINARY_DIR} for all files and not just fs.c
# because CMake strips include paths from the COMPILE_FLAGS source file property
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # to find fs.img
# set_source_files_properties(src/exception.S PROPERTIES
#     COMPILE_FLAGS "-I${CMAKE_CURRENT_BINARY_DIR}/foo"
# )

set(LIBKERNEL_SRCS
    src/cp0.c
    src/kernel_printf.c
    src/kernel_utils.c
    src/uart.c
)

set(BOOT_SRCS
    src/boot/boot.c
    src/boot/alloc.c
    src/boot/boot_printf.c
    src/boot/boot_utils.c
    src/boot/caches.c
    src/boot/elf_loader.c
    src/boot/stats.c
    src/boot/ufs_module.c
    src/boot/ufs_read.c
)
set(BOOT_ASM_SRCS
    src/boot/fs.S
    src/boot/init.S
)

set(NANO_ASM_SRCS
    nanokernel/src/memcpy_c.S
    nanokernel/src/nano_kernel.S
    nanokernel/src/ccall_soft.S
)

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND LIBKERNEL_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND LIBKERNEL_SRCS src/uart_altera.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_executable(cherios LINKER_SCRIPT boot.ld SOURCES
    ${LIBKERNEL_SRCS}
    ${BOOT_SRCS}
    ${BOOT_ASM_SRCS}
    ${NANO_ASM_SRCS}
)

# Create the file system
# HACK to make sure that file system is rebuilt after one of the targets changes
list(LENGTH CHERIOS_FILESYSTEM_DEPENDS _fs_dep_count)
message(STATUS "Filesystem requires ${_fs_dep_count} files: ${CHERIOS_FILESYSTEM_DEPENDS}")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.img DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} VERBATIM
    COMMAND mkdir -p "${CHERIOS_FILESYSTEM_DIR}" # make sure the directory exists first
    # create the timestamp file
    COMMAND date > "${CHERIOS_FILESYSTEM_DIR}/t1"
    # now build the file system
    # note: if you get "No space left on device" error, increase the -b percentage
    COMMAND ${MAKEFS_PROGRAM} -M 1k -b 5% -t ffs -o version=2 -B big "${CMAKE_CURRENT_BINARY_DIR}/fs.img" "${CHERIOS_FILESYSTEM_DIR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(filesystem DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs.img)
