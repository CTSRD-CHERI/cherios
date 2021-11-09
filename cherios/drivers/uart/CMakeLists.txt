
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(UART_SRCS
    ${INIT_ASM}
    src/main.c
    src/uart.c
    ${UART_CONSOLE}
)

add_cherios_executable_early(uart ADD_TO_FILESYSTEM LINKER_SCRIPT sandbox.ld SOURCES ${UART_SRCS})
