include_directories(include)

set(KERNEL_SRCS
    src/act.c
    src/cp0.c
    src/gc.c
    src/interrupts.c
    src/kernel_exceptions.c
    src/kernel_utils.c
    src/msg.c
    src/regdump.c
    src/sched.c
    src/syscalls.c
    src/timer.c
    src/uart.c
    src/critical.c
)
set(KERNEL_ASM_SRCS
    src/exception.S
    src/init.S
    src/ccall_soft.S
    src/ccall_trampoline.S
    src/context_switch.S
)
file(GLOB KERNEL_CHERIBSD_SRCS "src/cheribsd/*.c")
file(GLOB KERNEL_BAREMETAL_SRCS "src/baremetal/*.c")

if("${CONSOLE}" STREQUAL "malta")
    list(APPEND KERNEL_SRCS src/uart_malta.c)
elseif("${CONSOLE}" STREQUAL "altera")
    list(APPEND KERNEL_SRCS src/uart_altera.c)
else()
    message(FATAL_ERROR "Invalid choice for CONSOLE: ${CONSOLE}")
endif()

add_cherios_executable(kernel ADD_TO_FILESYSTEM LINKER_SCRIPT kernel.ld SOURCES
    ${KERNEL_SRCS}
    ${KERNEL_BAREMETAL_SRCS}
    ${KERNEL_ASM_SRCS}
)
#set_target_properties(kernel PROPERTIES COMPILE_FLAGS "-mllvm -cheri-no-global-bounds")
add_executable(kernel.o ${KERNEL_SRCS} ${KERNEL_CHERIBSD_SRCS})
set_target_properties(kernel.o PROPERTIES LINK_FLAGS "-r")
