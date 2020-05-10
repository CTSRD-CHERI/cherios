include_directories(include)
include_directories(${DRIVERS_DIR}/uart/include)
include_directories(${DRIVERS_DIR}/virtio/include)
include_directories(${DRIVERS_DIR}/megacore/include)
include_directories(${DRIVERS_DIR}/alteraSD/include)
include_directories(${CMAKE_SOURCE_DIR}/elf/include)
include_directories(${CMAKE_SOURCE_DIR}/tmpalloc/include)

set(LIBCRT_SRCS
    ${CMAKE_SOURCE_DIR}/libcrt/libc/freebsd_ports/subr_prf.c
    ${CMAKE_SOURCE_DIR}/libcrt/libc/freebsd_ports/strlen.c
    ${CMAKE_SOURCE_DIR}/libcrt/libc/freebsd_ports/memset.c
    ${CMAKE_SOURCE_DIR}/libcrt/libc/memcpy_l.S
    ${CMAKE_SOURCE_DIR}/libcrt/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtendC.c
)

set(COMMON_SRCS
    ${CMAKE_SOURCE_DIR}/elf/src/elf_loader.c
    src/plat.c
    src/caches.c
)

set(BOOT_SRCS
    src/boot/boot.c
    ${DRIVERS_DIR}/uart/src/uart.c
    src/boot/boot_printf.c
    src/boot/boot_utils.c
)

set(BOOT_ASM_SRCS
    src/boot/boot_tramp.S
    src/boot/boot_init.S
    src/boot/kernel_elf.S
    src/boot/init_elf.S
    src/boot/nano_elf.S
)

set(NANO_ASM_SRCS
    nanokernel/src/nano_kernel.S
    nanokernel/src/ccall_soft.S
    ${CMAKE_SOURCE_DIR}/sha256/src/sha256_k.S
)

set(INIT_SRCS
    src/init/init.c
    src/init/init_utils.c
    ${CMAKE_SOURCE_DIR}/tmpalloc/src/alloc.c
    ${CMAKE_SOURCE_DIR}/elf/src/cprogram.c
    ${CMAKE_CURRENT_BINARY_DIR}/fs.S
    ${CMAKE_CURRENT_BINARY_DIR}/init_fs.h
)

set(INIT_ASM_SRCS
    src/init/init.S
)

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND BOOT_SRCS ${DRIVERS_DIR}/uart/src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND BOOT_SRCS ${DRIVERS_DIR}/uart/src/uart_altera.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

# we have to include ${CMAKE_CURRENT_BINARY_DIR} for all files and not just fs.c
# because CMake strips include paths from the COMPILE_FLAGS source file property
include_directories(${CHERIOS_FILESYSTEM_DIR}) # to find files fir fs.S
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # to find init_fs.h

set_source_files_properties(src/boot/kernel_elf.S PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/kernel.elf"
)

set_source_files_properties(src/boot/init_elf.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/init.elf"
)

set_source_files_properties(src/boot/nano_elf.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/nano.elf"
)

add_cherios_init(init FREESTANDING LINKER_SCRIPT init.ld SOURCES
    ${COMMON_SRCS}
    ${INIT_SRCS}
    ${INIT_ASM_SRCS}
)

add_cherios_boot(cherios FREESTANDING LINKER_SCRIPT boot.ld SOURCES
    ${BOOT_ASM_SRCS}
    ${LIBCRT_SRCS}
    ${COMMON_SRCS}
    ${BOOT_SRCS}
)

add_cherios_nano(nano FREESTANDING LINKER_SCRIPT nano.ld SOURCES
    ${NANO_ASM_SRCS}
)

add_dependencies(cherios init)
add_dependencies(cherios kernel)
add_dependencies(cherios nano)

target_compile_definitions(cherios PUBLIC -DCHERIOS_BOOT)

# Create the file system
# HACK to make sure that file system is rebuilt after one of the targets changes
list(LENGTH CHERIOS_FILESYSTEM_DEPENDS _fs_dep_count)
message(STATUS "Filesystem requires ${_fs_dep_count} files: ${CHERIOS_FILESYSTEM_DEPENDS}")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.S ${CMAKE_CURRENT_BINARY_DIR}/init_fs.h
        DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} ${CMAKE_SOURCE_DIR}/boot/generate_fs.py VERBATIM
  COMMAND mkdir -p "${CHERIOS_FILESYSTEM_DIR}" # make sure the directory exists first
  # create the timestamp file
  COMMAND date > "${CHERIOS_FILESYSTEM_DIR}/t1"
  # now build the file system
  # note: if you get "No space left on device" error, increase the -b percentage
  COMMAND ls -l "${CHERIOS_FILESYSTEM_DIR}/t1"
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/boot/generate_fs.py ${CHERIOS_FILESYSTEM_DIR} ${CMAKE_CURRENT_BINARY_DIR}/fs.S ${CMAKE_CURRENT_BINARY_DIR}/init_fs.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(filesystem DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs.S ${CMAKE_CURRENT_BINARY_DIR}/init_fs.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf DEPENDS kernel VERBATIM
  COMMAND cp "$<TARGET_FILE:kernel>" "${CMAKE_CURRENT_BINARY_DIR}/kernel.elf"
)
add_custom_target(kernel_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf)

add_custom_target(init_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/init.elf)

add_custom_target(nano_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nano.elf)
