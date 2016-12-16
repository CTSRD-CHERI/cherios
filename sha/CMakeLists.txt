
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/main.c
    src/sha.c
)

execute_process(COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/sha_input_large.asc" "sha_input_large.asc")

add_cherios_executable(sha ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(sha PROPERTIES COMPILE_FLAGS -mxgot)
