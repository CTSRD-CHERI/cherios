cmake_minimum_required(VERSION 3.4)
# We have to set LANGUAGES to none so that we can set CMAKE_C_COMPILER
# before it checks the C compiler features. We can then use enable_language()
# to initialize all the C and ASM rules
project(CheriOS LANGUAGES NONE VERSION 0.0.1)

#
# Configure hardare at compile time.  Default to "qemu" for CHERI QEMU MALTA.
# Other option are "fpga" for CHERI on FPGA and "l3" for the L3 simulator.
#
set(HARDWARE "qemu" CACHE STRING
"Configure hardare at compile time.  Default to \"qemu\" for CHERI QEMU MALTA.\
 Other option are \"fpga\" for CHERI on FPGA and \"l3\" for the L3 simulator.")

# Set CHERI_SDK_DIR to the CHERI_SDK_DIR env variable if it is not given yet
set(CHERI_SDK_DIR $ENV{CHERI_SDK_DIR} CACHE PATH "Path to CHERI SDK")

if(NOT CHERI_SDK_DIR)
    # guess the location of CHERI SDK by looking for CheriSDKConfig and checking some guessed locations
    set(GUESSED_CHERISDK_LOCATIONS
        $ENV{HOME}/sdk/sdk
        $ENV{HOME}/cheri/output/sdk
        $ENV{HOME}/cheri/output/sdk256
    )
    # try to find clang in one of the guessed locations
    foreach(guess ${GUESSED_CHERISDK_LOCATIONS})
        if(EXISTS "${guess}/bin/clang")
            set(CHERI_SDK_DIR ${guess} CACHE PATH "Path to CHERI SDK" FORCE)
            break()
        endif()
    endforeach()
endif()

message(STATUS "CHERI_SDK_DIR set to: ${CHERI_SDK_DIR}")
if(CHERI_SDK_DIR)
    # first look for the CheriSDKConfig.cmake file installed by cheribuild.py
    # if not fall back to finding the sdk build by build_sdk.sh
    unset(CheriSDK_DIR) # force find_package() to run again in case we change CHERI_SDK_DIR
    unset(CheriSDK_FOUND) # force find_package() to run again in case we change CHERI_SDK_DIR
    find_package(CheriSDK QUIET CONFIG HINTS ${CHERI_SDK_DIR}/share/cmake/CheriSDK NO_DEFAULT_PATH)
    if(CheriSDK_FOUND)
        message(STATUS "Found CheriSDKConfig.cmake in ${CheriSDK_DIR}")
    else()
        # message(STATUS "Couldn't find CheriSDKConfig.cmake, setting variables manually")
        set(CheriSDK_SYSROOT_DIR "${CHERI_SDK_DIR}/sysroot")
        set(CheriSDK_TOOLCHAIN_DIR "${CHERI_SDK_DIR}/bin")
        set(CheriSDK_CC "${CheriSDK_TOOLCHAIN_DIR}/clang")
        set(CheriSDK_CXX "${CheriSDK_TOOLCHAIN_DIR}/clang++")
    endif()
endif()

if(EXISTS "${CheriSDK_CC}")
    message(STATUS "Found CHERI Clang: ${CheriSDK_CC}")
else()
    message(FATAL_ERROR "CHERI Clang ${CheriSDK_CC} does not exist!")
endif()

find_program(MAKEFS_PROGRAM makefs)
if(NOT MAKEFS_PROGRAM)
    message(FATAL_ERROR "Could not find makefs program, please install it!")
endif()

set(CMAKE_ASM_COMPILER ${CheriSDK_CC})
set(CMAKE_ASM_COMPILER_ID "Clang")  # for some reason CMake doesn't detect this automatically
set(CMAKE_C_COMPILER ${CheriSDK_CC})
set(CMAKE_CXX_COMPILER ${CheriSDK_CXX})
# make sure we compile for cheri:
set(CMAKE_ASM_COMPILER_TARGET cheri-unknown-freebsd)
set(CMAKE_C_COMPILER_TARGET cheri-unknown-freebsd)
set(CMAKE_CXX_COMPILER_TARGET cheri-unknown-freebsd)
# sysroot should not be needed as we are in a freestanding environment
# set(CMAKE_SYSROOT ${CheriSDK_SYSROOT_DIR})

# For some reason ranlib doesn't like our static library:
#   ranlib: libuser.a: File format is ambiguous
#   ranlib: Matching formats: elf64-tradbigmips ecoff-bigmips ecoff-littlemips
#   libuser/CMakeFiles/user.dir/build.make:468: recipe for target 'libuser/libuser.a' failed
# As it is not required we can simply use : instead
set(CMAKE_RANLIB : CACHE INTERNAL "noop for ranlib")

# If we don't set -nostartfiles -nodefaultlibs in CMAKE_EXE_LINKER_FLAGS,
# CMake will try to link ctr1.o into the executable and this might not exist
# We don't need it as we are building in a freestanding environment here anyway
# adding -cheri-linker here causes a segfault when detecting compiler features! TODO: report bug
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -nodefaultlibs"
    CACHE INTERNAL "Flags used by the linker" FORCE)
cmake_policy(SET CMP0056 NEW) # add CMAKE_EXE_LINKER_FLAGS when using try_compile()
enable_language(C)
enable_language(ASM)

include(CMakeParseArguments)
include(CheckCSourceCompiles)
check_c_source_compiles([===[
#if !__has_feature(capabilities)
#error need compiler with capability support
#endif
int main(int argc, char** argv) { return 0; }
]===] HAVE_CAPABILITIES)
if(NOT HAVE_CAPABILITIES)
    message(FATAL_ERROR "Compiler doesn't have capabilities support, set CHERI_SDK_DIR correctly.")
endif()

#
# Configure a console driver at compile time.  Default to the UART found in
# the MALTA reference board ("malta").  Other option is "altera" for the
# ALTERA JTAG UART used for BERI on FPGA.
#
if("${HARDWARE}" STREQUAL "qemu")
    set(CONSOLE "malta")
elseif("${HARDWARE}" STREQUAL "fpga")
    set(CONSOLE "altera")
elseif("${HARDWARE}" STREQUAL "l3")
    set(CONSOLE "altera")
else()
    message(FATAL_ERROR "Invalid choice for HARDWARE: ${HARDWARE}")
endif()

include_directories(include)
# set up the right flags for C and ASM sources
set(ASMC_COMPILER_FLAGS
    -mabi=sandbox
    -cheri-linker
    -integrated-as
    -msoft-float
    -fcolor-diagnostics
    # "-B${CheriSDK_TOOLCHAIN_DIR}"  # Doesn't seem to be required
)
set(ASM_COMPILER_FLAGS
    ${ASMC_COMPILER_FLAGS}
    -mllvm -asm-macro-max-nesting-depth=100 -fno-pic
)
set(C_COMPILER_FLAGS
    ${ASMC_COMPILER_FLAGS}
    -nostdinc
    -mcpu=cheri
    -mllvm
    -mxmxgot
    -O0 # TODO: make this depend on the configuration
    -fno-pic
    -Werror
    -std=c11
    -DHARDWARE_${HARDWARE}
    -DCONSOLE_${CONSOLE}
)
set(C_WARNING_FLAGS
    -Wall
    -Wextra
    -Weverything

    # Explicitly error out on constructs that would always result in a crash.
    -Werror=cheri-capability-misuse
    -Werror=implicit-function-declaration
    -Werror=format
    -Werror=undefined-internal

    -Wcast-qual
    -Wdisabled-optimization
    -Winit-self
    -Winline
    -Wpointer-arith
    -Wredundant-decls
    -Wshadow
    -Wswitch-default
    -Wundef
    -Wwrite-strings

    -Wno-error=cast-align
    -Wno-error=cast-qual
    -Wno-error=comma
    -Wno-error=conversion
    -Wno-error=covered-switch-default
    -Wno-error=dollar-in-identifier-extension
    -Wno-error=expansion-to-defined
    -Wno-error=extra-semi
    -Wno-error=missing-noreturn
    -Wno-error=missing-prototypes
    -Wno-error=missing-variable-declarations
    -Wno-error=padded
    -Wno-error=pedantic
    -Wno-error=reserved-id-macro
    -Wno-error=sign-conversion
    -Wno-error=switch-enum
    -Wno-error=unreachable-code
    -Wno-error=unreachable-code-break
    -Wno-error=unreachable-code-return
    -Wno-error=unused-function
    -Wno-error=unused-macros
    -Wno-error=unused-parameter
    -Wno-error=unused-variable
    -Wno-error=used-but-marked-unused
    -Wno-error=vla
)
set(C_COMPILER_FLAGS ${C_COMPILER_FLAGS} ${C_WARNING_FLAGS})

# Use the strange CMake generator expressions to set per-language compile flags
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_COMPILER_FLAGS}>")

# TODO: only add -g when CMAKE_BUILD_TYPE == Debug
# for now we always want it
add_compile_options(-g)

set(CHERIOS_FILESYSTEM_DIR "${CMAKE_BINARY_DIR}/fs")
set(CHERIOS_FILESYSTEM_DEPENDS "" CACHE INTERNAL "dependencies for filesystem")

# Usage: add_cherios_executable(target [ADD_TO_FILESYSTEM] LINKERSCRIPT sandbox.ld SOURCES src1...)
function(add_cherios_executable _target)
	add_cherios_executable_low(${_target} ${ARGN})
	target_link_libraries(${_target} CheriOS::LibUser)
endfunction()

function(add_cherios_executable_low _target)
    cmake_parse_arguments(add_exe "ADD_TO_FILESYSTEM" "LINKER_SCRIPT" "SOURCES" ${ARGN})
    if(NOT add_exe_LINKER_SCRIPT)
        message(FATAL_ERROR "Missing LINKER_SCRIPT parameter!")
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/ldscripts/${add_exe_LINKER_SCRIPT}")
        message(FATAL_ERROR "Chosen LINKER_SCRIPT ${add_exe_LINKER_SCRIPT} does not exist!")
    endif()
    set(_srcs ${add_exe_SOURCES})
    list(LENGTH _srcs _src_count)
    message("Adding executable ${_target} with ${_src_count} source files and linker script ${add_exe_LINKER_SCRIPT}")
    if("${_src_count}" LESS 1)
        message(FATAL_ERROR "No sources passed to add_cherios_executable()")
    endif()

    add_executable(${_target} ${_srcs})
    # Tell the compiler to use the specified linker script
    target_link_libraries(${_target}
        "-L${CMAKE_SOURCE_DIR}/ldscripts"
        "-T${add_exe_LINKER_SCRIPT}"
    )
    target_link_libraries(${_target}
        -cheri-linker
        -nostartfiles
        -nodefaultlibs
    )
    # add the linker scripts as dependencies and set the output file name
    # TODO: don't hardcode mips.ld as an additional file dependency
    set_target_properties(${_target} PROPERTIES
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/ldscripts/${add_exe_LINKER_SCRIPT};${CMAKE_SOURCE_DIR}/ldscripts/mips.ld"
        OUTPUT_NAME "${_target}.elf"
    )
    # TODO: don't hardcode common.ld as an additional file dependency
    set_target_properties(${_target} PROPERTIES
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/ldscripts/${add_exe_LINKER_SCRIPT};${CMAKE_SOURCE_DIR}/ldscripts/common.ld"
        OUTPUT_NAME "${_target}.elf"
    )

    # add it to the filesystem dir if requested
    if(add_exe_ADD_TO_FILESYSTEM)
        set(_fs_target_path "${CHERIOS_FILESYSTEM_DIR}/${_target}.elf")
        add_custom_command(TARGET ${_target} POST_BUILD
            # make sure the directory exists
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CHERIOS_FILESYSTEM_DIR}"
            # copy the binary to the fs root directory
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${_target}> "${_fs_target_path}"
            # remove some sections to save place (TODO: make this depend on the configuration)
            COMMAND "${CheriSDK_TOOLCHAIN_DIR}/strip" "${_fs_target_path}"
            COMMAND "${CheriSDK_TOOLCHAIN_DIR}/objcopy" -R .comment -R .pdr -R .apple_names -R .apple_objc -R .apple_namespaces -R .apple_types -R .MIPS.options -R .MIPS.abiflags "${_fs_target_path}"
        )
        # Using a CACHE INTERNAL variable ensures that changes are globally visible
        set(CHERIOS_FILESYSTEM_DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} ${_target} CACHE INTERNAL "dependencies for filesystem")
    endif()
    # create a .dump file of the binary (added to byproducts so that ninja clean removes it)
    add_custom_command(TARGET ${_target} POST_BUILD BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump"
        COMMAND "${CheriSDK_TOOLCHAIN_DIR}/objdump" -x -d -S "$<TARGET_FILE:${_target}>" > "${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump"
    )
endfunction()


add_subdirectory(libuser)

add_subdirectory(fatfs)
add_subdirectory(hello)
add_subdirectory(memmgt)
add_subdirectory(namespace)
add_subdirectory(prga)
add_subdirectory(sockets)
add_subdirectory(test1a)
add_subdirectory(test1b)
add_subdirectory(test2a)
add_subdirectory(test2b)
add_subdirectory(test3)
add_subdirectory(uart)
add_subdirectory(virtio-blk)
add_subdirectory(zlib)
add_subdirectory(zlib_test)

add_subdirectory(kernel)
add_subdirectory(boot)
