get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(X_SRCS
    ${SECURE_INIT_ASM}
    src/main.c
    src/sockets.c
)

add_cherios_executable_light(${ACT_NAME} ADD_TO_FILESYSTEM TEMPORAL_A LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
target_compile_definitions(${ACT_NAME} PUBLIC -DLIGHTWEIGHT_OBJECT=1)
