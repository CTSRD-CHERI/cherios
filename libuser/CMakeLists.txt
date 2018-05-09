set(LIBCRT_SRCS
    ${CMAKE_SOURCE_DIR}/libcrt/memcpy_c.S
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
)

set(LIBUSER_ASM_SRCS
    src/init.S
    src/msg.S
    src/exceptions.S
    ${PLT_ASM}
)

set(LIBUSER_SRCS
    src/assert.c
    src/ctype.c
    src/capmalloc/capmalloc_bump.c
    src/errno.c
    src/libuser.c
    src/mmap.c
    src/msg.c
    src/msg_default.c
    src/namespace.c
    src/object.c
    src/panic.c
    src/printf.c
    src/ssleep.c
    src/statcounters.c
    src/stdio.c
    src/stdlib.c
    src/virtioblk.c
    src/thread.c
    src/act_events.c
    src/type_man.c
    src/exceptions.c
    src/dedup.c
    src/temporal.c
    src/sockets.c
    src/unistd.c
)

add_library(user STATIC ${LIBCRT_SRCS} ${LIBUSER_SRCS} ${LIBUSER_ASM_SRCS})
add_library(CheriOS::LibUser ALIAS user)
add_library(libcrt STATIC ${LIBCRT_SRCS})
add_library(CheriOS::LibCRT ALIAS libcrt)