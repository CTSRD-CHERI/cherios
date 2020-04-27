get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(X_SRCS
    ${INIT_ASM}
    src/main.c
)

add_cherios_executable(app ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
target_link_libraries(app PRIVATE lib1)
target_link_libraries(app PRIVATE lib2)