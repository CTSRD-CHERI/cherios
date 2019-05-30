include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NAMESPACE_SRCS
    ${INIT_ASM}
    src/main.c
    src/namespace.c
)

add_cherios_namespace(namespace ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${NAMESPACE_SRCS})
