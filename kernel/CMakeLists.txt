include_directories(include)

set(LIBKERNEL_ASM_SRCS
    src/exception.S
    src/fs.S
    src/init.S
)

# fs.S depends on the fs.img file
set_source_files_properties(src/fs.S PROPERTIES
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

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND LIBKERNEL_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND LIBKERNEL_SRCS src/uart_altera.c)
elseif("${CONSOLE}" STREQUAL "gxemul")
    list(APPEND LIBKERNEL_SRCS src/uart_gxemul.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_executable(cherios LINKER_SCRIPT kernel.ld SOURCES
    ${LIBKERNEL_SRCS}
    ${LIBKERNEL_ASM_SRCS}
    ${KERNEL_SRCS}
    ${KERNEL_ASM_SRCS}
)

# Create the file system
# HACK to make sure that file system is rebuilt after one of the targets changes
list(LENGTH CHERIOS_FILESYSTEM_DEPENDS _fs_dep_count)
message(STATUS "Filesystem requires ${_fs_dep_count} files: ${CHERIOS_FILESYSTEM_DEPENDS}")

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    # Don't make the fs too large, it might break the whole elf without warning
    # Max tested ok: 1M TODO:find limit TODO:remove limit
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.img DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} VERBATIM
        COMMAND mkdir -p "${CHERIOS_FILESYSTEM_DIR}" # make sure the directory exists first
        # create the timestamp file
        COMMAND date > "${CHERIOS_FILESYSTEM_DIR}/t1"
        # now build the file system
        COMMAND makefs -s 256k -t ffs -o version=2 -B big "${CMAKE_CURRENT_BINARY_DIR}/fs.img" "${CHERIOS_FILESYSTEM_DIR}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
else()
    message(WARNING "Cannot create file system on non-FreeBSD (yet)!")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.img DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} VERBATIM
        COMMAND echo "WARNING: cannot create file system on non-FreeBSD (yet)!"
        # create a dummy file
        COMMAND echo 123456 > "${CMAKE_CURRENT_BINARY_DIR}/fs.img"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

add_custom_target(filesystem DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs.img)
