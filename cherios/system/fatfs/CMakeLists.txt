
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(FATFS_SRCS
    ${DEFAULT_INSECURE_ASM}
    src/main.c
    src/diskio.c
    src/ff.c
    src/ff_sync.c
)

add_cherios_executable(fatfs ADD_TO_FILESYSTEM FREESTANDING CAN_VARY_SS
                       LINKER_SCRIPT sandbox.ld SOURCES ${FATFS_SRCS})
