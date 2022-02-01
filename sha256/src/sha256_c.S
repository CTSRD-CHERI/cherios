#ifdef PLATFORM_mips
#include "platform/mips/sha256.S"
#else
#include "platform/riscv/sha256.S"
#endif