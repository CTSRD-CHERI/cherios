set(LIBZ_SRCS
    adler32.c
    crc32.c
    deflate.c
    infback.c
    inffast.c
    inflate.c
    inftrees.c
    trees.c
    zutil.c
    compress.c
    uncompr.c
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
)

add_library(z STATIC ${LIBZ_SRCS})
set_target_properties(z PROPERTIES COMPILE_FLAGS -mxgot)
add_library(CheriOS::LibZ ALIAS z)
