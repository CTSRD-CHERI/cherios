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


add_subdirectory(libuser)
add_subdirectory(kernel)
