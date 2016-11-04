include_directories(include)

set(KERNEL_SRCS
    src/act.c
    src/ccall.c
    src/cp0.c
    src/gc.c
    src/init.c
    src/interrupts.c
    src/kernel_exceptions.c
    src/kernel_printf.c
    src/kernel_utils.c
    src/msg.c
    src/regdump.c
    src/sched.c
    src/syscalls.c
    src/timer.c
    src/uart.c
)
set(KERNEL_ASM_SRCS
    src/exception.S
    src/init.S
)

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND KERNEL_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND KERNEL_SRCS src/uart_altera.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_kernel(kernel LINKER_SCRIPT kernel.ld SOURCES
    ${KERNEL_SRCS}
    ${KERNEL_ASM_SRCS}
)
#set_target_properties(kernel PROPERTIES COMPILE_FLAGS "-mllvm -cheri-no-global-bounds")
