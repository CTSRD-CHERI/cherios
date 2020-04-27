get_filename_component(ACT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(X_SRCS
    src/main.c
)

add_cherios_library(lib1 ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${X_SRCS})
