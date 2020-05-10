get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

make_usage(user)

set(X_SRCS
    ${INIT_ASM}
    user_usage.S
    src/main.c
)

add_cherios_executable(${ACT_NAME} ADD_TO_FILESYSTEM FREESTANDING
                       LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
