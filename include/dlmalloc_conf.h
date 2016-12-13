/* Config for CheriOS */
#include "mips.h"
#include "assert.h"
#define DEBUG 1
#define ABORT_ON_ASSERT_FAILURE 0
#define ONLY_MSPACES 1

//#define LACKS_ERRNO_H
#define LACKS_FCNTL_H
//#define LACKS_SYS_MMAN_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_TYPES_H
#define LACKS_TIME_H
#define LACKS_UNISTD_H

#include "debug.h"
#define TRACE(s) syscall_puts(s)

/* fixme: dlmalloc tries to merge mmap allocation -> does not work with cheri
   mmap forces a shadow page for now */
