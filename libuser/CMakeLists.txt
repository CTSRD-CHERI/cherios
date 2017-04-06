set(LIBUSER_ASM_SRCS
    src/init.S
    src/memcpy_c.S
    src/msg.S
)

set(LIBUSER_SRCS
    src/crtbeginC.c
    src/crtendC.c
    src/assert.c
    src/ctype.c
    src/debug.c
    src/dlmalloc.c
    src/errno.c
    src/interrupt.c
    src/libuser.c
    src/malloc.c
    src/memset.c
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
    src/strlen.c
    src/subr_prf.c
    src/virtioblk.c
)

add_library(user STATIC ${LIBUSER_SRCS} ${LIBUSER_ASM_SRCS})
set_target_properties(user PROPERTIES COMPILE_FLAGS -mxgot)
add_library(CheriOS::LibUser ALIAS user)

add_executable(act_msg.o src/cheribsd/msg.S)
set_target_properties(act_msg.o PROPERTIES COMPILE_FLAGS -mxgot LINK_FLAGS "-r")
