set(CMAKE_CROSSCOMPILING ON)
cmake_minimum_required(VERSION 3.13)

# We have to set LANGUAGES to none so that we can set CMAKE_C_COMPILER
# before it checks the C compiler features. We can then use enable_language()
# to initialize all the C and ASM rules
project(CheriOS LANGUAGES NONE VERSION 0.0.1)

# Apparently this needs to come after project(...)
set(CMAKE_SYSTEM_NAME "CheriOS" CACHE STRING "Target system" FORCE)
set(CMAKE_SYSTEM_PROCESSOR "Cheri" CACHE STRING "Target CPU" FORCE)

SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS true)

# Allow override of options using set
cmake_policy(SET CMP0077 NEW)

# Configure hardware at compile time.  Default to "qemu" for CHERI QEMU MALTA.
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
        /local/scratch/$ENV{USER}/cheri/output/cherios-sdk
        $ENV{HOME}/cherios-sdk/
        $ENV{HOME}/cheri/output/cherios-sdk
    )
    # try to find clang in one of the guessed locations
    foreach(guess ${GUESSED_CHERISDK_LOCATIONS})
        if(EXISTS "${guess}/bin/clang")
            set(CHERI_SDK_DIR ${guess} CACHE PATH "Path to CHERI SDK" FORCE)
            break()
        endif()
    endforeach()
endif()

message(STATUS "CHERI_SDK_DIR set to: '${CHERI_SDK_DIR}'")
if(CHERI_SDK_DIR)
    # first look for the CheriSDKConfig.cmake file installed by cheribuild.py
    # if not fall back to finding the sdk build by build_sdk.sh
    unset(CheriSDK_DIR) # force find_package() to run again in case we change CHERI_SDK_DIR
    unset(CheriSDK_FOUND) # force find_package() to run again in case we change CHERI_SDK_DIR
    find_package(CheriSDK QUIET CONFIG HINTS ${CHERI_SDK_DIR}/share/cmake/CheriSDK NO_DEFAULT_PATH)
    if(CheriSDK_FOUND)
        message(STATUS "Found CheriSDKConfig.cmake in ${CheriSDK_DIR}")
    else()
        message(STATUS "Couldn't find CheriSDKConfig.cmake, setting variables manually")
        set(CheriSDK_SYSROOT_DIR "${CHERI_SDK_DIR}/sysroot")
        set(CheriSDK_TOOLCHAIN_DIR "${CHERI_SDK_DIR}/bin")
        set(CheriSDK_CC "${CheriSDK_TOOLCHAIN_DIR}/clang")
        set(CheriSDK_CXX "${CheriSDK_TOOLCHAIN_DIR}/clang++")
        set(CheriSDK_AR "${CheriSDK_TOOLCHAIN_DIR}/llvm-ar")
        set(CheriSDK_RANLIB "${CheriSDK_TOOLCHAIN_DIR}/llvm-ranlib")
    endif()
endif()

find_package(Python3)

# Threading is built into CheriOS's libuser

set(CMAKE_THREAD_LIBS_INIT "")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(CMAKE_THREAD_PREFER_PTHREAD ON)

if(EXISTS "${CheriSDK_CC}")
    message(STATUS "Found CHERI Clang: ${CheriSDK_CC}")
else()
    message(FATAL_ERROR "CHERI Clang ${CheriSDK_CC} does not exist!")
endif()

if("${PLATFORM}" STREQUAL "mips" OR "${PLATFORM}" STREQUAL "mips64")
    set(PLATFORM mips)
    set(TARGET_TRIP mips64-unknown-freebsd)
    set(ABI_NAME purecap)
    set(PLATFORM_ARGS -msoft-float)
    # Historically, we could choose between `cheri128` and `cheri256`
    set(CHERI_CPU cheri128)
    set(BAREBONES 0)
elseif("${PLATFORM}" STREQUAL "riscv" OR "${PLATFORM}" STREQUAL "riscv64")
    set(PLATFORM riscv)
    set(TARGET_TRIP riscv64-unknown-freebsd)
    set(ABI_NAME l64pc128)
    set(PLATFORM_ARGS -mno-relax)
    set(PLATFORM_ARGS2 -mxcheri-rvc)
    set(CHERI_CPU rv64imacxcheri)
    set(BAREBONES 1)
else()
    message(FATAL_ERROR "Unknown platform: ${PLATFORM}")
endif()

set(CAP_BYTES 16)

set(CMAKE_ASM_COMPILER ${CheriSDK_CC})
set(CMAKE_ASM_COMPILER_ID "Clang")  # for some reason CMake doesn't detect this automatically
set(CMAKE_C_COMPILER ${CheriSDK_CC})
set(CMAKE_AR ${CheriSDK_AR})
set(CMAKE_RANLIB ${CheriSDK_RANLIB})
set(CMAKE_CXX_COMPILER ${CheriSDK_CXX})
# make sure we compile for cheri:
set(CMAKE_ASM_COMPILER_TARGET ${TARGET_TRIP})
set(CMAKE_C_COMPILER_TARGET ${TARGET_TRIP})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIP})
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

# The --target flag line is needed so that CLion can detect the include paths (it doesn't add the CMAKE_C_COMPILER_TARGET)
set(CMAKE_ASM_FLAGS_INIT "--target=${TARGET_TRIP} ${PLATFORM_ARGS} -march=${CHERI_CPU} -mabi=${ABI_NAME}")
set(CMAKE_C_FLAGS_INIT "--target=${TARGET_TRIP} ${PLATFORM_ARGS} -march=${CHERI_CPU} -mabi=${ABI_NAME}")
set(CMAKE_CXX_FLAGS_INIT "--target=${TARGET_TRIP} ${PLATFORM_ARGS} -march=${CHERI_CPU} -mabi=${ABI_NAME}")
set(CMAKE_EXE_LINKER_FLAGS "--target=${TARGET_TRIP} ${PLATFORM_ARGS} -nostartfiles -nodefaultlibs -fuse-ld=lld -march=${CHERI_CPU} -mabi=${ABI_NAME}"
    CACHE INTERNAL "Flags used by the linker" FORCE)
cmake_policy(SET CMP0056 NEW) # add CMAKE_EXE_LINKER_FLAGS when using try_compile()

enable_language(C)
enable_language(CXX)
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
    if (${PLATFORM} STREQUAL "mips")
        set(CONSOLE "malta")
    else()
        set(CONSOLE "16550")
    endif()
elseif("${HARDWARE}" STREQUAL "fpga")
    set(CONSOLE "altera")
elseif("${HARDWARE}" STREQUAL "l3")
    set(CONSOLE "altera")
else()
    message(FATAL_ERROR "Invalid choice for HARDWARE: ${HARDWARE}")
endif()

option(MSGDMA "Use altera MSGDMA for the ethernet" OFF)

if("${MSGDMA}")
    set(MSGDMA_FLAGS -DSGDMA)
endif()

# Configure Multicore

set(SMP_CORES "1" CACHE STRING "Number of virtual cores for target system")

if("${SMP_CORES}" STREQUAL "1" OR (NOT ${SMP_CORES}))
    set(SMP_FLAGS
    )
else()
    set(SMP_FLAGS
            -DSMP_ENABLED
            -DSMP_CORES=${SMP_CORES}
    )
endif()

# Configure net

option(BUILD_WITH_NET "build with a network stack" ON)

if(BUILD_WITH_NET)
    set(BNET_FLAGS
            -DBUILD_WITH_NET=1)
else()
    set(BNET_FLAGS
            -DBUILD_WITH_NET=0)
endif()

# Add the stripes

option(GO_FAST "Turn off all debugging features, paint on go fast stripes" OFF)
option(ALWAYS_LTO "Always use LTO" OFF)

if(GO_FAST)
    set(BFAST_FLAGS -DGO_FAST=1)
else()
    set(BFAST_FLAGS -DGO_FAST=0)
endif()

set(VARY_SECURE "UNCHANGED" CACHE STRING "Force secure load and distrust calling ON/OFF for programs that support it")
set(VARY_SS "UNCHANGED" CACHE STRING "Force safe stacks ON/OFF for programs that support it")
set(EXPAND_LIMIT "64" CACHE STRING "Default expand limit for temporal analysis")

if(VARY_SS STREQUAL "OFF")
    set(VARY_FLAGS -DUNSAFE_STACKS_OFF=1)
else()
    set(VARY_FLAGS -DUNSAFE_STACKS_OFF=0)
endif()

if(VARY_SECURE STREQUAL "OFF")
    set(VARY_SECURE_FLAGS -DFORCE_INSECURE=1)
elseif(VARY_SECURE STREQUAL "ON")
    set(VARY_SECURE_FLAGS -DFORCE_SECURE=1)
endif()

# These files make use of include_next and so need be listed FIRST
#include_directories(libcxx/include)

include_directories(SYSTEM include)
include_directories(SYSTEM include/sys)
include_directories(elf/include)
include_directories(elf/include/platform/${PLATFORM})
include_directories(sha256/include)
include_directories(sha256/include/platform/${PLATFORM})
include_directories(cherios/system/dylink/include)
include_directories(cherios/system/dylink/include/platform/${PLATFORM})
include_directories(cherios/system/lwip/include)
include_directories(cherios/system/fatfs/include)
include_directories(libcrt/include)
include_directories(lwip/src/include)

# Include the platform specific directories
include_directories(SYSTEM include/platform/${PLATFORM})

set(O_LVL 3)

# set up the right flags for C and ASM sources
set(ASMC_COMPILER_FLAGS
    -DHARDWARE_${HARDWARE}
    ${SMP_FLAGS}
    ${BNET_FLAGS}
    ${BFAST_FLAGS}
    ${VARY_FLAGS}
    ${VARY_SECURE_FLAGS}
    -march=${CHERI_CPU}
    -march=${CHERI_CPU}
    -mabi=${ABI_NAME}
    -isysroot=/dev/null # Remove the default '/usr/include' one...
    -DASM_VISIBILITY=.hidden
    -DPLATFORM_${PLATFORM}
    -DBAREBONES=${BAREBONES}
    -fvisibility=hidden
    "SHELL:-mllvm -is-cherios"
    -march=${CHERI_CPU}
    -cheri-cap-table-abi=plt
    -integrated-as
    ${PLATFORM_ARGS}
    ${PLATFORM_ARGS2}
    -fcolor-diagnostics
    -fpic
    # "-B${CheriSDK_TOOLCHAIN_DIR}"  # Doesn't seem to be required
)
set(ASM_COMPILER_FLAGS
    ${ASMC_COMPILER_FLAGS}
    "SHELL:-mllvm -asm-macro-max-nesting-depth=100"
)
set(C_COMPILER_FLAGS
    ${ASMC_COMPILER_FLAGS}
    -ftls-model=local-exec
    -fomit-frame-pointer
    -O${O_LVL} # TODO: make this depend on the configuration
    -DCONSOLE_${CONSOLE}
    ${MSGDMA_FLAGS}
    -g
)
set(C_WARNING_FLAGS
    -Wall
    -Wextra
    # -Weverything // Clang broken with this

    # Explicitly error out on constructs that would always result in a crash.
    -Werror=cheri-capability-misuse
    -Werror=implicit-function-declaration
    -Werror=format
    -Werror=undefined-internal
    -Werror
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

    # We are the operating system, we are allowed to use underscores.
    # There's also nothing wrong with those dollars, or padding, or not
    # explicitly handling each and every enum when there's the default
    # clause.
    -Wno-reserved-id-macro
    -Wno-dollar-in-identifier-extension
    -Wno-padded
    -Wno-switch-enum
    # So sue me
    -Wno-undef
    -Wno-gnu-variable-sized-type-not-at-end
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-char-subscripts
    -Wno-error=comment
    -Wno-unused-function
    -Wno-error=cast-align
    -Wno-error=cast-qual
    -Wno-error=comma
    -Wno-error=conversion
    -Wno-error=covered-switch-default
    -Wno-error=deprecated-declarations
    -Wno-error=expansion-to-defined
    -Wno-error=extra-semi
    -Wno-error=missing-prototypes
    -Wno-error=missing-variable-declarations
    -Wno-error=pedantic
    -Wno-error=sign-conversion
    -Wno-error=unreachable-code
    -Wno-error=unreachable-code-break
    -Wno-error=unreachable-code-return
    -Wno-error=unused-macros
    -Wno-deprecated-declarations
    -Werror=cheri-prototypes
)

set(CXX_WARNING_FLAGS
        -Wno-c++98-compat-pedantic
        -Wno-old-style-cast
        -Wno-c11-extensions
        )

set(CXX_COMPILER_FLAGS -std=c++14 -fno-rtti -fno-exceptions ${C_COMPILER_FLAGS} ${C_WARNING_FLAGS} ${CXX_WARNING_FLAGS})
set(C_COMPILER_FLAGS     -std=c11
        ${C_COMPILER_FLAGS} ${C_WARNING_FLAGS})

# Use the strange CMake generator expressions to set per-language compile flags
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_COMPILER_FLAGS}>")


set(CHERIOS_FILESYSTEM_DIR "${CMAKE_BINARY_DIR}/fs")
set(CHERIOS_FILESYSTEM_DEPENDS "" CACHE INTERNAL "dependencies for filesystem")

function(add_to_filesystem _target _fs_target_path)
  add_custom_command(TARGET ${_target} POST_BUILD
    # Make sure the directory exists
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CHERIOS_FILESYSTEM_DIR}"
    # Copy the binary to the filesystem root directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${_target}> "${_fs_target_path}"
    # remove some sections to save place (TODO: make this depend on the configuration)
    # NOTE: Had to comment these out as lld was not alligning offsets properly causing build failure.
    COMMAND "${CheriSDK_TOOLCHAIN_DIR}/strip" "${_fs_target_path}"
    # COMMAND "${CheriSDK_TOOLCHAIN_DIR}/objcopy" -R .comment -R .pdr -R .apple_names -R .apple_objc -R .apple_namespaces -R .apple_types -R .MIPS.options -R .MIPS.abiflags "${_fs_target_path}"
    )
  # Using a CACHE INTERNAL variable ensures that changes are globally visible
  set(CHERIOS_FILESYSTEM_DEPENDS ${CHERIOS_FILESYSTEM_DEPENDS} ${_target} CACHE INTERNAL "dependencies for filesystem")
endfunction()

set(LD_DIR ${CMAKE_SOURCE_DIR}/ldscripts/platform/${PLATFORM})

function(set_core_ld_deps _target)
    if("${PLATFORM}" STREQUAL "mips")
        set_target_properties(${_target} PROPERTIES
                LINK_DEPENDS "${LD_DIR}/${add_exe_LINKER_SCRIPT};${LD_DIR}/mips.ld;;${LD_DIR}/common.ld"
                )
    else()
        set_target_properties(${_target} PROPERTIES
                LINK_DEPENDS "${LD_DIR}/${add_exe_LINKER_SCRIPT};${LD_DIR}/riscv.ld;;${LD_DIR}/common.ld"
                )
    endif()
endfunction()

# Usage: add_cherios_executable(target [ADD_TO_FILESYSTEM] [CAN_VARY_SS] [TEMPORAL_A] LINKERSCRIPT sandbox.ld SOURCES src1...)

# ADD_TO_FILESYSTEM will embed the program inside init so it can be loaded early. Currently I am very lazy and use this
# for every program. Eventually, only core programs should load this way, and others should come from a storage device.
# CAN_VARY_SS will allow safe stacks to vary on/off based on a global flag passed to cmake
# TEMPORAL_A will perform the temporal safety analysis pass (will force on LTO as well)

function(add_cherios_executable _target)
  add_cherios_module(${_target} ${ARGN})
  target_link_libraries(${_target} PRIVATE CheriOS::LibUser)
endfunction()

function(add_cherios_executable_early _target)
    add_cherios_module(${_target} FREESTANDING ${ARGN})
    target_link_libraries(${_target} PRIVATE CheriOS::LibUserEarly)
endfunction()

function(add_cherios_executable_light _target)
    add_cherios_module(${_target} ${ARGN})
    target_link_libraries(${_target} PRIVATE CheriOS::LibUserLight)
    target_compile_definitions(${_target} PUBLIC -DLIGHTWEIGHT_OBJECT=1)
endfunction()


function(add_cherios_kernel _target)
  add_cherios_module(${_target} ${ARGN})
  set_core_ld_deps(${_target})
  target_link_libraries(${_target} PRIVATE CheriOS::LibCRTKern)
  target_compile_definitions(${_target} PUBLIC -DIS_KERNEL=1)
  target_link_libraries(${_target} PRIVATE "-static")
endfunction()

function(add_cherios_nano _target)
 add_cherios_module(${_target} ${ARGN})
 set_core_ld_deps(${_target})
 target_compile_options(${_target} PUBLIC -fno-pic)
endfunction()

function(add_cherios_boot _target)
  add_cherios_module(${_target} ${ARGN})
  set_core_ld_deps(${_target})
  target_compile_definitions(${_target} PUBLIC -DIS_BOOT=1)
  target_link_libraries(${_target} PRIVATE "-static")
endfunction()

function(add_cherios_init _target)
  add_cherios_module(${_target} ${ARGN})
  set_core_ld_deps(${_target})
  target_link_libraries(${_target} PRIVATE CheriOS::LibUserEarly)
  target_link_libraries(${_target} PRIVATE "-static")
endfunction()

function(add_cherios_memmgr _target)
  add_cherios_module(${_target} FREESTANDING ${ARGN})
  set_core_ld_deps(${_target})
  target_link_libraries(${_target} PRIVATE CheriOS::LibUserEarly)
endfunction()

function(add_cherios_namespace _target)
  add_cherios_module(${_target} ${ARGN})
  set_core_ld_deps(${_target})
  target_link_libraries(${_target} PRIVATE CheriOS::LibUserEarly)
endfunction()

function(make_usage _target)
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}_usage.S
                        DEPENDS "${CMAKE_SOURCE_DIR}/generate_captable_usage.py" ${_target}
                        COMMAND "${CheriSDK_TOOLCHAIN_DIR}/llvm-nm" "$<TARGET_FILE:${_target}>" | ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/generate_captable_usage.py" > "${CMAKE_CURRENT_BINARY_DIR}/${_target}_usage.S"
    )
endfunction()

set(AUTOGENERATED_IDS_DIR "${CMAKE_BINARY_DIR}/ids")
file(MAKE_DIRECTORY "${AUTOGENERATED_IDS_DIR}")

function(make_id _target)
    add_custom_command(
            OUTPUT ${_target}_id.h
            DEPENDS "${CMAKE_SOURCE_DIR}/generate_found_id.py" ${_target}
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/generate_found_id.py" "$<TARGET_FILE:${_target}>" ${CAP_BYTES} 4 > "${AUTOGENERATED_IDS_DIR}/${_target}_id.h"
            )
    add_custom_target(
            ${_target}_id
            DEPENDS
            ${_target}_id.h
    )
endfunction()

# FIXME documentation: what is this for?
set(CLANG_LOAD "SHELL:-Xclang -load")
set(CLANG_LIB_T "SHELL:-Xclang ${CHERI_SDK_DIR}/lib/libTemporal.so")
set(CLANG_STATS "-save-stats=obj")
set(TEMP_A "SHELL:-mllvm -TemporalType=analysis")
set(TEMP_C "SHELL:-mllvm -TemporalType=capture_track")
set(TEMP_64 "SHELL:-mllvm -TemporalExpandLimit=64")
set(TEMP_16 "SHELL:-mllvm -TemporalExpandLimit=16")
set(TEMP_1 "SHELL:-mllvm -TemporalExpandLimit=1")


function(add_cherios_library _target)
    add_cherios_module(${_target} IS_DYN ${ARGN})
    # CheriOS dynamic libraries are themselves runnable standalone programs, but the program they run is a libk_serv.
    # The link_serv is a single-thread, lightweight, embedded executable that has the authority to create new
    # processes and threads for a library. Some basic functionality (e.g. memcpy) is shared between the link-serv and
    # the library proper. Therefore some symbols are always required to be a part of a dyanamic library.
    # When actually linking, a given process may override these symbols, but the library will continue to use its own
    target_link_libraries(${_target} PRIVATE link_serv)
    target_compile_definitions(${_target} PUBLIC -DLIGHTWEIGHT_OBJECT=1)
endfunction()

function(add_cherios_module _target)
  set(options ADD_TO_FILESYSTEM TEMPORAL_A USE_LTO CAN_VARY_SS NEED_LIBCXXRT NEED_LIBCXX IS_DYN FREESTANDING)
  set(oneValueArgs LINKER_SCRIPT)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(add_exe "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(${add_exe_CAN_VARY_SS})
      if("${VARY_SS}" STREQUAL "ON")
            set(add_exe_TEMPORAL_A "ON")
      elseif(${VARY_SS} STREQUAL "OFF")
            set(add_exe_TEMPORAL_A "OFF")
      endif()
  endif()

  if(${add_exe_TEMPORAL_A} AND ${PLATFORM} STREQUAL "riscv")
      message("WARN: disabling temporal analysis for RISCV")
      set(add_exe_TEMPORAL_A "OFF")
  endif()

  if(ALWAYS_LTO)
    set(add_exe_USE_LTO "ON")
  endif()

  if((NOT add_exe_LINKER_SCRIPT) OR (add_exe_LINKER_SCRIPT STREQUAL "sandbox.ld"))
    # There is no linker script needed for normal programs
    set(LINKER_SCRIPT_ARG "-e start -Wl,--image-base,0,--no-rosegment -z max-page-size=16")
  elseif(NOT EXISTS "${LD_DIR}/${add_exe_LINKER_SCRIPT}")
    message(FATAL_ERROR "Chosen LINKER_SCRIPT ${add_exe_LINKER_SCRIPT} does not exist!")
  else()
    set(LINKER_SCRIPT_ARG
            "-L${LD_DIR}"
            "-Wl,-T,${LD_DIR}/${add_exe_LINKER_SCRIPT}")
  endif()
  set(_srcs ${add_exe_SOURCES})
  list(LENGTH _srcs _src_count)
  message("Adding executable ${_target} with ${_src_count} source files and linker script ${add_exe_LINKER_SCRIPT}")
  if("${_src_count}" LESS 1)
    message(FATAL_ERROR "No sources passed to add_cherios_executable()")
  endif()

  if(add_exe_IS_DYN)
      add_library(${_target} SHARED ${_srcs})
  else()
      add_executable(${_target} ${_srcs})
  endif()

  # Tell the compiler to use the specified linker script. No need for a dynamic linker because we embed it.
  target_link_libraries(${_target}
        PRIVATE
        ${LINKER_SCRIPT_ARG}
        -fuse-ld=lld
        "-Wl,-Bdynamic -Wl,-no-pie,-is-cherios,-relative-cap-relocs,-no-dynamic-linker,--no-rosegment"
        -G0
        -nostartfiles
        -nodefaultlibs
        -mabi=${ABI_NAME}
        -Wl,-z,norelro
   )

  if(NOT add_exe_IS_DYN)
       set_target_properties(${_target} PROPERTIES OUTPUT_NAME "${_target}.elf")
  endif()

  if(${add_exe_USE_LTO} OR ${add_exe_TEMPORAL_A})
      message("Using LTO for ${_target}")
      # In order to do lto we need to pass all these flags
      target_link_libraries(${_target}
              PRIVATE
              -Wl,-mllvm,-is-cherios,-mllvm,-float-abi=soft,-mllvm,-march=${CHERI_CPU},-mllvm,-mattr=+soft-float,-mllvm,-mattr=+${CHERI_CPU},-mllvm,-target-abi=purecap,-mllvm,-cheri-cap-table-abi=plt,--lto-O${O_LVL}
      )
      target_compile_options(${_target} PUBLIC "-flto")
  endif()

  if(${add_exe_TEMPORAL_A})
      target_link_libraries(${_target}
           PRIVATE
          -Wl,-mllvm,-TemporalType=analysis,-mllvm,-TemporalExpandLimit=${EXPAND_LIMIT}
      )
  endif()

  # add it to the filesystem dir if requested
  if(add_exe_ADD_TO_FILESYSTEM)
      if(add_exe_IS_DYN)
          set(_output_extention "so")
      else()
          set(_output_extention "elf")
      endif()
    add_to_filesystem(${_target} "${CHERIOS_FILESYSTEM_DIR}/${_target}.${_output_extention}")
  endif()
  # create a .dump file of the binary (added to byproducts so that ninja clean removes it)
  #add_custom_command(TARGET ${_target} POST_BUILD BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump"
  #  COMMAND "${CheriSDK_TOOLCHAIN_DIR}/llvm-objdump" -d -S "$<TARGET_FILE:${_target}>" > "${CMAKE_CURRENT_BINARY_DIR}/${_target}.dump"
  #  )

    if(add_exe_NEED_LIBCXX)
        target_link_libraries(${_target} PRIVATE cxx_static)
    endif()

    if(add_exe_NEED_LIBCXXRT)
        target_link_libraries(${_target} PRIVATE cxxrt-static)
    endif()

    if(add_exe_FREESTANDING)
        target_compile_options(${_target} PRIVATE -ffreestanding)
    endif()
endfunction()

set(CHERIOS_DIR         "${CMAKE_SOURCE_DIR}/cherios")
set(DRIVERS_DIR         "${CHERIOS_DIR}/drivers")
set(INIT_ASM_DIR        "${CMAKE_SOURCE_DIR}/init_asm/platform/${PLATFORM}")
set(UFS_DIR             "${CMAKE_SOURCE_DIR}/ufs")
set(INIT_ASM            "${INIT_ASM_DIR}/init.S")
set(PLT_ASM             "${CHERIOS_DIR}/system/dylink/src/platform/${PLATFORM}/stubs.S")
set(SECURE_INIT_ASM     "${INIT_ASM_DIR}/secure_init.S")

if(${VARY_SECURE} STREQUAL "UNCHANGED")

    set(DEFAULT_SECURE_ASM ${SECURE_INIT_ASM})
    set(DEFAULT_INSECURE_ASM ${INIT_ASM})

elseif(${VARY_SECURE} STREQUAL "ON")

    set(DEFAULT_SECURE_ASM ${SECURE_INIT_ASM})
    set(DEFAULT_INSECURE_ASM ${SECURE_INIT_ASM})

elseif(${VARY_SECURE} STREQUAL "OFF")

    set(DEFAULT_SECURE_ASM ${INIT_ASM})
    set(DEFAULT_INSECURE_ASM ${INIT_ASM})

endif()

set (LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lwip)
include(lwip/src/Filelists.cmake)
set (LWIP_INCLUDE       "${CMAKE_SOURCE_DIR}/lwip/src/include")
include(cherios/drivers/uart/src/CMakeLists.txt)
if(NOT ${BAREBONES})

#
# Now add our various components
#

# Just include libcxx directly so it picks up the right compiler options
set(LIBCXX_ENABLE_SHARED OFF)
set(LIBCXX_ENABLE_EXPERIMENTAL_LIBRARY OFF)
set(LIBCXX_ENABLE_FILESYSTEM OFF)
set(LLVM_INCLUDE_TESTS OFF)
set(LIBCXX_INCLUDE_BENCHMARKS OFF)
set(LLVM_INCLUDE_DOCS OFF)
set(LIBCXX_ABI_FORCE_ITANIUM ON)
set(LIBCXX_ENABLE_EXCEPTIONS OFF)
set(LIBCXX_ENABLE_RTTI OFF)
set(LIBCXX_ENABLE_GLOBAL_FILESYSTEM_NAMESPACE OFF)
# Eventually we want this on, but that requires a sched.h plus some extra rubbish in time.h, which is a TODO
set(LIBCXX_ENABLE_THREADS OFF)
set(LIBCXX_ENABLE_THREAD_UNSAFE_C_FUNCTIONS OFF)
set(LIBCXX_ENABLE_MONOTONIC_CLOCK OFF)

set(PACKAGE_VERSION 11.0.0git)

add_subdirectory(libcxx)

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_BINARY_DIR}/libcxx/__config_site>")
include_directories(BEFORE PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/libcxx/include>")

endif()

add_subdirectory(libuser)

add_subdirectory(libcxxrt)

add_subdirectory(cherios)

if(NOT ${BAREBONES})

add_subdirectory(hello)
#add_subdirectory(zlib)
#add_subdirectory(zlib_test)

add_subdirectory(churn)
add_subdirectory(foundation_test)
add_subdirectory(exception_test)
add_subdirectory(unsafe_test)
add_subdirectory(dedup_test)
add_subdirectory(socket_test)
add_subdirectory(fs_test)
add_subdirectory(unaligned_test)

add_subdirectory(server)
add_subdirectory(client)

add_subdirectory(nginx)
#nginx has too many
target_compile_options(nginx PRIVATE
        -Wno-cast-qual
        -Wno-incompatible-pointer-types-discards-qualifiers
        -Wno-unused-parameter
        -Wno-pointer-sign
        -Wno-cheri-bitwise-operations
        -Wno-unused-variable
        -Wno-sign-compare
        -Wno-incompatible-pointer-types)

add_subdirectory(top)
add_subdirectory(nc_shell)
add_subdirectory(snake)

add_subdirectory(benchmarks)
add_subdirectory(demos)

#add_subdirectory(prga)
#add_subdirectory(sockets)
#add_subdirectory(test1a)
#add_subdirectory(test1b)
#add_subdirectory(test2a)
#add_subdirectory(test2b)
#add_subdirectory(test3)
add_subdirectory(pthread_test)

add_subdirectory(cpptest)
add_subdirectory(dylink_test)

endif()

add_subdirectory(boot)
