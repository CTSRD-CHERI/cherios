include_directories(include)

set(NANOKERNEL_SRCS

)
set(NANOKERNEL_ASM_SRCS
    ccall_soft.S
    nano_kernel.S
)

add_cherios_executable(nanokernel ADD_TO_FILESYSTEM LINKER_SCRIPT nanokernel.ld SOURCES
    ${NANOKERNEL_SRCS}
    ${NANOKERNEL_ASM_SRCS}
)
#set_target_properties(nanokernel PROPERTIES COMPILE_FLAGS "-mllvm -cheri-no-global-bounds")
