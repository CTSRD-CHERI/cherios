
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NAMESPACE_SRCS
    src/init.S
    src/main.c
    src/namespace.c
)

add_cherios_executable(namespace ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${NAMESPACE_SRCS})
