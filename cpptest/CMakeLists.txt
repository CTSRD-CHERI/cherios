get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(X_SRCS
        ${INIT_ASM}
        src/main.cpp
        )

add_cherios_executable(${ACT_NAME} ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
