
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/adpcm.c
    src/main.c
)

execute_process(COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/adpcm_large.pcm" "adpcm_large.pcm")
execute_process(COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/adpcm_large.adpcm" "adpcm_large.adpcm")

add_cherios_executable(adpcm ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
