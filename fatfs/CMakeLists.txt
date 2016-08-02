
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(FATFS_SRCS
    src/init.S
    src/main.c
    src/diskio.c
    src/ff.c
)

add_cherios_executable(fatfs ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${FATFS_SRCS})
