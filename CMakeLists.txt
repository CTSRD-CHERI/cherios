cmake_minimum_required(VERSION 3.4)
# We have to set LANGUAGES to none so that we can set CMAKE_C_COMPILER
# before it checks the C compiler features. We can then use enable_language()
# to initialize all the C and ASM rules
project(CheriOS LANGUAGES NONE VERSION 0.0.1)

# first look for the CheriSDKConfig.cmake file installed by cheribuild.py
# if not fall back to finding the sdk build by build_sdk.sh
if(CHERI_SDK_DIR)
    # CheriSDK_DIR should point to where the CMake config file is stored
    set(CheriSDK_DIR "${CHERI_SDK_DIR}/share/cmake/CheriSDK")
    message(STATUS "Looking for CheriSDKConfig.cmake in ${CheriSDK_DIR}")
endif()
find_package(CheriSDK QUIET HINTS
    $ENV{HOME}/sdk/sdk
    $ENV{HOME}/cheri/output/sdk
    $ENV{HOME}/cheri/output/sdk256
)
if(CheriSDK_FOUND)
    message(STATUS "Found CheriSDKConfig.cmake in ${CheriSDK_DIR}")
else()
    if(NOT CHERI_SDK_DIR)
        message(FATAL_ERROR "Could not determine location of CHERI SDK. Set CHERI_SDK_DIR to the SDK root directory.")
    endif()
    message(STATUS "Couldn't find CheriSDKConfig.cmake, setting variables manually")
    set(CheriSDK_SYSROOT_DIR "${CHERI_SDK_DIR}/sysroot")
    set(CheriSDK_TOOLCHAIN_DIR "${CHERI_SDK_DIR}/bin")
    set(CheriSDK_CC "${CheriSDK_TOOLCHAIN_DIR}/clang")
    set(CheriSDK_CXX "${CheriSDK_TOOLCHAIN_DIR}/clang++")
    if(NOT EXISTS "${CheriSDK_CC}")
        message(FATAL_ERROR "CHERI_SDK_DIR set but could not find clang at ${CheriSDK_CC}")
    endif()
endif()

set(CMAKE_ASM_COMPILER ${CheriSDK_CC})
set(CMAKE_ASM_COMPILER_ID "Clang")  # for some reason CMake doesn't detect this automatically
set(CMAKE_C_COMPILER ${CheriSDK_CC})
set(CMAKE_CXX_COMPILER ${CheriSDK_CXX})
set(CMAKE_SYSROOT ${CheriSDK_SYSROOT_DIR})
enable_language(C)
enable_language(ASM)

# now that we have set up the compile flags
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

include_directories(include)
# set up the right flags for C and ASM sources
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
    -O2 # TODO: make this depend on the configuration
    -G0
    -mxgot
    -Werror
    -std=c11
)
set(C_WARNING_FLAGS
    -Wall
    -Wextra
    -Wdisabled-optimization
    -Wformat=2
    -Winit-self
    -Winline
    -Wpointer-arith
    -Wredundant-decls
    -Wswitch-default
    -Wswitch-enum
    -Wundef
    -Wwrite-strings
    -Wshadow
#    -Wcast-align
#    -Wcast-qual
#    -Wconversion

    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-parameter
)
set(C_COMPILER_FLAGS ${C_COMPILER_FLAGS} ${C_WARNING_FLAGS})

# Use the strange CMake generator expressions to set per-language compile flags
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_COMPILER_FLAGS}>")

# TODO: only add -g when CMAKE_BUILD_TYPE == Debug
# for now we always want it
add_compile_options(-g)

# Usage: add_cherios_executable(target LINKERSCRIPT sandbox.ld SOURCES src1...)
function(add_cherios_executable _target)
    cmake_parse_arguments(add_exe "" "LINKER_SCRIPT" "SOURCES" ${ARGN})
    message(STATUS "add_exe_LINKER_SCRIPT: ${add_exe_LINKER_SCRIPT}")
    if(NOT add_exe_LINKER_SCRIPT)
        message(FATAL_ERROR "Missing LINKER_SCRIPT parameter!")
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/ldscripts/${add_exe_LINKER_SCRIPT}")
        message(FATAL_ERROR "Chosen LINKER_SCRIPT ${add_exe_LINKER_SCRIPT} does not exist!")
    endif()
    set(_srcs ${add_exe_SOURCES})
    list(LENGTH _srcs _src_count)
    message("Adding executable ${_target} with ${_src_count} source files")
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
        -G0
        -mxgot
        -nostartfiles
        -nodefaultlibs
    )
    # add the linker scripts as dependencies and set the output file name
    # TODO: don't hardcode mips.ld as an additional file dependency
    set_target_properties(${_target} PROPERTIES
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/ldscripts/${add_exe_LINKER_SCRIPT};${CMAKE_SOURCE_DIR}/ldscripts/mips.ld"
        OUTPUT_NAME "${_target}.elf"
    )
    # create the dump file:
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump
        DEPENDS ${_target}
        COMMAND "${CheriSDK_TOOLCHAIN_DIR}/objdump" -x -d -S "$<TARGET_FILE:${_target}>" > "${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_custom_target(${_target}_dump ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump)
    target_link_libraries(${_target} CheriOS::LibUser)
endfunction()

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

add_subdirectory(libuser)
add_subdirectory(prga)
add_subdirectory(sockets)
add_subdirectory(uart)
add_subdirectory(kernel)
