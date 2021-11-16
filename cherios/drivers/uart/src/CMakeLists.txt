# This file just exists to define the source file that should be directly included
# by anything that wants uart (boot/kernel/uart driver etc). It should only be included
# by the toplevel cmake.



if("${CONSOLE}" STREQUAL "malta")
    set(UART_CONSOLE ${CMAKE_SOURCE_DIR}/cherios/drivers/uart/src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    set(UART_CONSOLE ${CMAKE_SOURCE_DIR}/cherios/drivers/uart/src/uart_altera.c)
elseif("${CONSOLE}" STREQUAL "qemu_debug")
    set(UART_CONSOLE ${CMAKE_SOURCE_DIR}/cherios/drivers/uart/src/uart_qemu_debug.c)
elseif("${CONSOLE}" STREQUAL "16550")
    set(UART_CONSOLE ${CMAKE_SOURCE_DIR}/cherios/drivers/uart/src/uart_16550.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()
