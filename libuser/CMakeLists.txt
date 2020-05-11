set (LIBC_ROOT ${CMAKE_SOURCE_DIR}/libcrt/libc)


# Try to keep important files early, it helps deduplication when other heuristics fail
set(LIBCRT_SRCS
    ${LIBC_ROOT}/memcpy_l.S
    ${LIBC_ROOT}/freebsd_ports/memset.c
    ${LIBC_ROOT}/freebsd_ports/strlen.c
    ${LIBC_ROOT}/freebsd_ports/subr_prf.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtbeginC.c
    ${CMAKE_SOURCE_DIR}/libcrt/crtendC.c
    ${CMAKE_SOURCE_DIR}/libcrt/cp0.c
    ${LIBC_ROOT}/freebsd_ports/strcmp.c
    ${LIBC_ROOT}/mem_cmp.c
    ${LIBC_ROOT}/atoi.c
# String stuff
    ${LIBC_ROOT}/strchr.c
    ${LIBC_ROOT}/freebsd_ports/strstr.c
    ${LIBC_ROOT}/freebsd_ports/strbprk.c
    ${LIBC_ROOT}/freebsd_ports/qsort.c
    ${LIBC_ROOT}/freebsd_ports/strcat.c
    ${LIBC_ROOT}/freebsd_ports/memchr.c
    ${LIBC_ROOT}/freebsd_ports/strcpy.c
    ${LIBC_ROOT}/strdup.c
    ${LIBC_ROOT}/freebsd_ports/strtol.c
    ${LIBC_ROOT}/freebsd_ports/strncpy.c
    ${LIBC_ROOT}/freebsd_ports/strrchr.c
    ${LIBC_ROOT}/freebsd_ports/strcspn.c
# Maths (currently nothing provided, built ins should cover common cases)
# Wide chars

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
    src/unaligned.c
    src/msg.c
    src/type_man.c
    src/thread.c
    src/pthread.c
    src/pthread_mutex.c
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
    ${LIBC_ROOT}/memcpy_l.S
    ${LIBC_ROOT}/freebsd_ports/memset.c
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
    src/unaligned.c
    src/msg.c
    src/type_man.c
    src/errno.c
    src/lightdummies.c
)

set(LIBUSER_CLIENT ${CMAKE_SOURCE_DIR}/cherios/system/dylink/src/client.c)
set(LIBUSER_SERVER ${CMAKE_SOURCE_DIR}/cherios/system/dylink/src/server.c)

add_library(user_early STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS})
target_compile_definitions(user_early PUBLIC -DUSE_SYSCALL_PUTS -DLIB_EARLY)
add_library(CheriOS::LibUserEarly ALIAS user_early)

add_library(user STATIC ${LIBCRT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_SRCS} ${LIBUSER_CLIENT})
add_library(CheriOS::LibUser ALIAS user)

add_library(user_light STATIC ${LIBCRT_LIGHT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_LIGHT_SRCS})
target_compile_definitions(user_light PUBLIC -DLIGHTWEIGHT_OBJECT=1)
add_library(CheriOS::LibUserLight ALIAS user_light)


add_library(link_serv STATIC ${LIBCRT_LIGHT_SRCS} ${LIBUSER_ASM_SRCS} ${LIBUSER_LIGHT_SRCS} ${INIT_ASM}
        ${LIBUSER_CLIENT} ${LIBUSER_SERVER} ${LIBC_ROOT}/freebsd_ports/strlen.c ${LIBC_ROOT}/atoi.c)
target_link_libraries(link_serv PRIVATE "-Wl,-Bdynamic -Wl,-no-pie")
target_compile_definitions(link_serv PRIVATE -DLIGHTWEIGHT_OBJECT=1)
target_compile_options(link_serv PRIVATE -fvisibility=hidden)

add_library(libcrt STATIC ${LIBCRT_SRCS})
add_library(CheriOS::LibCRT ALIAS libcrt)

add_library(libcrt_kern STATIC ${LIBCRT_SRCS})
target_compile_definitions(libcrt_kern PUBLIC -DIS_KERNEL=1)
add_library(CheriOS::LibCRTKern ALIAS libcrt_kern)