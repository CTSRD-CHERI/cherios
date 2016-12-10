include_directories(include)

set(LIB_SRCS
    ${CMAKE_SOURCE_DIR}/lib/subr_prf.c
    ${CMAKE_SOURCE_DIR}/lib/strlen.c
    ${CMAKE_SOURCE_DIR}/lib/memset.c
    ${CMAKE_SOURCE_DIR}/lib/memcpy_c.S
    ${CMAKE_SOURCE_DIR}/lib/memcpy.c
    ${CMAKE_SOURCE_DIR}/lib/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/lib/crtendC.c
    ${CMAKE_SOURCE_DIR}/lib/strcmp.c
)

set(COMMON_SRCS
    src/elf_loader.c
    src/plat.c
    src/caches.c
)

set(BOOT_SRCS
    src/cp0.c
    src/boot/boot.c
    src/boot/uart.c
    src/boot/boot_printf.c
    src/boot/boot_utils.c
)

set(BOOT_ASM_SRCS
    src/boot/boot_init.S
    src/boot/kernel_elf.S
    src/boot/init_elf.S
    src/boot/trampoline.S
)

set(INIT_SRCS
    src/init/init.c
    src/init/init_utils.c
    src/init/init_main.c
    src/init/alloc.c
    src/init/stats.c
    src/init/ufs_module.c
    src/init/ufs_read.c
    src/init/glue.c
)

set(INIT_ASM_SRCS
    src/init/fs.S
    src/init/init_init.S
)

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND BOOT_SRCS src/boot/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND BOOT_SRCS src/boot/uart_altera.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

# we have to include ${CMAKE_CURRENT_BINARY_DIR} for all files and not just fs.c
# because CMake strips include paths from the COMPILE_FLAGS source file property
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # to find fs.img

# fs.S depends on the fs.img file, kernel_elf.S on kernel.elf, init_elf.S on init.elf
set_source_files_properties(src/init/fs.S PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fs.img"
)
set_source_files_properties(src/boot/kernel_elf.S PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/kernel.elf"
)
set_source_files_properties(src/boot/init_elf.S PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/init.elf"
)

add_cherios_init(init LINKER_SCRIPT init.ld SOURCES
    ${LIB_SRCS}
    ${COMMON_SRCS}
    ${INIT_SRCS}
    ${INIT_ASM_SRCS}
)

add_cherios_boot(cherios LINKER_SCRIPT boot.ld SOURCES
    ${LIB_SRCS}
    ${COMMON_SRCS}
    ${BOOT_SRCS}
    ${BOOT_ASM_SRCS}
)
set_target_properties(cherios PROPERTIES COMPILE_FLAGS -DCHERIOS_BOOT)

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
  COMMAND ls -l "${CHERIOS_FILESYSTEM_DIR}/t1"
  COMMAND ${MAKEFS_PROGRAM} -M 1k -b 10% -t ffs -o version=2 -B big "${CMAKE_CURRENT_BINARY_DIR}/fs.img" "${CHERIOS_FILESYSTEM_DIR}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(filesystem DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs.img)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf DEPENDS kernel VERBATIM
  COMMAND cp "$<TARGET_FILE:kernel>" "${CMAKE_CURRENT_BINARY_DIR}/kernel.elf"
)
add_custom_target(kernel_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf)

add_custom_target(init_elf DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/init.elf)
