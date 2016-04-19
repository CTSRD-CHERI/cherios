set(LIBUSER_ASM_SRCS
    src/init.S
    src/memcpy_c.S
)

set(LIBUSER_SRCS
    src/crtbeginC.c
    src/crtendC.c
    src/assert.c
    src/ctype.c
    src/debug.c
    src/libuser.c
    src/malloc.c
    src/memset.c
    src/object.c
    src/printf.c
    src/ssleep.c
    src/strcmp.c
    src/strlen.c
    src/subr_prf.c
)

add_library(user STATIC ${LIBUSER_SRCS} ${LIBUSER_ASM_SRCS})
