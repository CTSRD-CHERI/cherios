get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(X_SRCS
    ${DEFAULT_SECURE_ASM}
    src/main.c
    src/aes.c
)

add_cherios_executable(${ACT_NAME} ADD_TO_FILESYSTEM TEMPORAL_A CAN_VARY_SS LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
