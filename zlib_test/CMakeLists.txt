
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/main.c
    src/zlib.c
)

add_cherios_executable(zlib_test ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
target_link_libraries(zlib_test CheriOS::LibUser)
