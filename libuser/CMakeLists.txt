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
)

set(LIBUSER_ASM_SRCS
    src/init.S
    src/msg.S
    src/exceptions.S
    ${PLT_ASM}
)

set(LIBUSER_SRCS
    src/statcounters.c
    src/assert.c
    src/printf.c
    src/object.c
    src/virtioblk.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
    src/ctype.c
    src/errno.c
    src/libuser.c
    src/mmap.c
    src/msg.c
    src/panic.c
    src/ssleep.c
    src/stdio.c
    src/stdlib.c
    src/thread.c
    src/act_events.c
    src/type_man.c
    src/net.c
    src/dedup.c
    src/temporal.c
    src/unistd.c
    src/namespace.c
    src/exceptions.c
    src/capmalloc/capmalloc_bump.c
    src/sockets.c
    src/plt_allocations.c
)

add_library(user_early STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS})
add_library(CheriOS::LibUserEarly ALIAS user_early)
set_target_properties(user_early PROPERTIES COMPILE_FLAGS -DUSE_SYSCALL_PUTS)

add_library(user STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS})
add_library(CheriOS::LibUser ALIAS user)

add_library(libcrt STATIC ${LIBCRT_SRCS})
add_library(CheriOS::LibCRT ALIAS libcrt)