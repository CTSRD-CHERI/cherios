
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/main.c
    src/aes.c
)

add_cherios_executable(AES ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(AES PROPERTIES COMPILE_FLAGS -mxgot)
