
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/init.S
    src/main.c
)

execute_process(COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/AES_input_large.asc" "AES_input_large.asc")

add_cherios_executable(spam ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${SRCS})
set_target_properties(spam PROPERTIES COMPILE_FLAGS -mxgot)
