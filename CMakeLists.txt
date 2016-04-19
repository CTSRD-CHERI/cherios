cmake_minimum_required(VERSION 3.4)
# We have to set LANGUAGES to none so that we can set CMAKE_C_COMPILER
# before it checks the C compiler features. We can then use enable_language()
# to initialize all the C and ASM rules
project(CheriOS LANGUAGES NONE VERSION 0.0.1)
find_package(CheriSDK REQUIRED PATHS
    $ENV{HOME}/sdk/sdk
    $ENV{HOME}/cheri/output/sdk
    $ENV{HOME}/cheri/output/sdk256
)
set(CMAKE_ASM_COMPILER ${CheriSDK_CC})
set(CMAKE_ASM_COMPILER_ID "Clang")
set(CMAKE_C_COMPILER ${CheriSDK_CC})
set(CMAKE_CXX_COMPILER ${CheriSDK_CXX})
set(CMAKE_SYSROOT ${CheriSDK_SYSROOT_DIR})
enable_language(C)
enable_language(ASM)

include_directories(include)

add_subdirectory(kernel)
