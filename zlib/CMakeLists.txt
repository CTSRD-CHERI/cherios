include(ExternalProject)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    ${INIT_ASM}
    src/main.c
)

add_cherios_module(zlib ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(zlib PROPERTIES COMPILE_FLAGS -mxgot)
add_subdirectory(zlib)

target_link_libraries(zlib CheriOS::LibZ)
target_link_libraries(zlib CheriOS::LibUser)
