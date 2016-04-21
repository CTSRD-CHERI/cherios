
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(UART_SRCS
    src/init.S
    src/main.c
    src/printf.c
    src/uart.c
)

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND UART_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND UART_SRCS src/uart_altera.c)
elseif("${CONSOLE}" STREQUAL "gxemul")
    list(APPEND UART_SRCS src/uart_gxemul.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_executable(uart LINKER_SCRIPT sandbox.ld SOURCES ${UART_SRCS})
