set(LIBCRT_SRCS
    ${CMAKE_SOURCE_DIR}/libcrt/memcpy.S
    ${CMAKE_SOURCE_DIR}/libcrt/memset.c
    ${CMAKE_SOURCE_DIR}/libcrt/strlen.c
    ${CMAKE_SOURCE_DIR}/libcrt/subr_prf.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtendC.c
    ${CMAKE_SOURCE_DIR}/libcrt/cp0.c
    ${CMAKE_SOURCE_DIR}/libcrt/strcmp.c
    ${CMAKE_SOURCE_DIR}/libcrt/mem_cmp.c
    ${CMAKE_SOURCE_DIR}/libcrt/atoi.c
    ${CMAKE_SOURCE_DIR}/libcrt/strchr.c
    ${CMAKE_SOURCE_DIR}/libcrt/strstr.c
    ${CMAKE_SOURCE_DIR}/libcrt/strbprk.c
    ${CMAKE_SOURCE_DIR}/libcrt/qsort.c
    ${CMAKE_SOURCE_DIR}/libcrt/strcat.c
    ${CMAKE_SOURCE_DIR}/libcrt/memchr.c
    ${CMAKE_SOURCE_DIR}/libcrt/strcpy.c
)

set(LIBUSER_ASM_SRCS
    src/init.S
    src/msg.S
    src/exceptions.S
    ${PLT_ASM}
    src/invoke_function_pointer.S
)

set(LIBUSER_SRCS
    src/plt_allocations.c
    src/libuser.c
    src/object.c
    src/dedup.c
    src/printf.c
    src/assert.c
    src/stdio.c
    src/sockets.c
    src/capmalloc/capmalloc_bump.c
    src/mmap.c
    src/exceptions.c
    src/namespace.c
    src/temporal.c
    src/msg.c
    src/type_man.c
    src/thread.c
    src/panic.c
    src/stdlib.c
    src/unistd.c
    src/ctype.c
    src/ssleep.c
    src/errno.c
    src/net.c
    src/dylink.c
    src/virtioblk.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
    src/act_events.c
)

# Single thread. Exceptions, message sending and mmap. Meant to be used to make link servers work.
# On the way to proper dynamic linking.

set(LIBCRT_LIGHT_SRCS
    ${CMAKE_SOURCE_DIR}/libcrt/memcpy.S
    ${CMAKE_SOURCE_DIR}/libcrt/memset.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtendC.c
)

set(LIBUSER_LIGHT_SRCS
    src/plt_allocations.c
    src/libuser.c
    src/object.c
    src/mmap.c
    src/exceptions.c
    src/namespace.c
    src/temporal.c
    src/msg.c
    src/type_man.c
    src/errno.c
    src/lightdummies.c
)

add_library(user_early STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS})
target_compile_definitions(user_early PUBLIC -DUSE_SYSCALL_PUTS)
add_library(CheriOS::LibUserEarly ALIAS user_early)

add_library(user STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS})
add_library(CheriOS::LibUser ALIAS user)

add_library(user_light STATIC ${LIBCRT_LIGHT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_LIGHT_SRCS})
target_compile_definitions(user_light PUBLIC -DLIGHTWEIGHT_OBJECT=1)
add_library(CheriOS::LibUserLight ALIAS user_light)

add_library(libcrt STATIC ${LIBCRT_SRCS})
add_library(CheriOS::LibCRT ALIAS libcrt)