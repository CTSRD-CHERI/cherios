include_directories(
        include
        include/platform/${PLATFORM}
        ${CMAKE_SOURCE_DIR}/boot/include/boot
        ${DRIVERS_DIR}/uart/include)

if(GO_FAST)
    set(KERNEL_DEBUG_SRCS)
else()
    set(KERNEL_DEBUG_SRCS
            ${DRIVERS_DIR}/uart/src/uart.c
            ${UART_CONSOLE}
            )
endif()

set(KERNEL_SRCS
    src/act.c
    src/init.c
    src/interrupts.c
    src/kernel_exceptions.c
    src/platform/${PLATFORM}/kernel_exceptions.c
    src/kernel_printf.c
    src/kernel_utils.c
    src/platform/${PLATFORM}/kernel_utils.c
    src/msg.c
    src/platform/${PLATFORM}/regdump.c
    src/sched.c
    src/syscalls.c
    src/timer.c
    src/mutex.c
    ${KERNEL_DEBUG_SRCS}
)
set(KERNEL_ASM_SRCS
    src/platform/${PLATFORM}/init.S
    src/platform/${PLATFORM}/message_send.S
    ${PLT_ASM}
)

add_cherios_kernel(kernel LINKER_SCRIPT kernel.ld SOURCES
    ${KERNEL_SRCS}
    ${KERNEL_ASM_SRCS}
)
#set_target_properties(kernel PROPERTIES COMPILE_FLAGS "-mllvm -cheri-no-global-bounds")
