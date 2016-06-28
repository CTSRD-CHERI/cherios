#include "mips.h"

typedef enum module_type {
	m_memmgt,
	m_namespace,
	m_uart,
	m_core,
	m_user,
	m_fence
} module_t;

/*
 * Bootloader routines
 */
void	boot_alloc_init(void);
void	boot_alloc_enable_system(void * ctrl);
void *	boot_alloc(size_t s);
void	boot_free(void * p);

void *	elf_loader(const char * s);
void *	load (const char * filename, int * len);

void *	load_module(module_t type, const char * file, int arg);

void	hw_init(void);
void	install_exception_vectors(void);

void	glue_memmgt(void * memmgt_ctrl, void* ns_ctrl);
