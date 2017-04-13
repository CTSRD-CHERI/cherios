set(LIBCRT_SRCS
    ${CMAKE_SOURCE_DIR}/libcrt/memcpy_c.S
    ${CMAKE_SOURCE_DIR}/libcrt/memcpy.S
    ${CMAKE_SOURCE_DIR}/libcrt/memset.c
    ${CMAKE_SOURCE_DIR}/libcrt/strlen.c
    ${CMAKE_SOURCE_DIR}/libcrt/subr_prf.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtendC.c
    ${CMAKE_SOURCE_DIR}/libcrt/cp0.c
)

set(LIBUSER_ASM_SRCS
    src/init.S
    src/msg.S
    src/ccall.S
)

set(LIBUSER_SRCS
    src/assert.c
    src/ctype.c
    src/dlmalloc.c
    src/errno.c
    src/libuser.c
    src/malloc.c
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
    src/strcmp.c
    src/virtioblk.c
)

add_library(user STATIC ${LIBCRT_SRCS} ${LIBUSER_SRCS} ${LIBUSER_ASM_SRCS})
set_target_properties(user PROPERTIES COMPILE_FLAGS -mxgot)
add_library(CheriOS::LibUser ALIAS user)
