#ifndef _BOOT_H_
#define _BOOT_H_

#include "mips.h"
#include "cdefs.h"
#include "stdio.h"

typedef enum module_type {
	m_memmgt,
	m_namespace,
	m_uart,
	m_fs,
	m_core,
	m_user,
	m_fence
} module_t;

typedef struct boot_elem_s {
	module_t     type;
	int          cond;
	const char * name;
	register_t   arg;
	int          daemon;
	int          status;
	void 	   * ctrl;
} boot_elem_t;

extern void	kernel_trampoline;
extern void	kernel_trampoline_end;

extern char	__start_heap;
extern char	__stop_heap;

extern void	__boot_load_virtaddr;
extern void	__kernel_load_virtaddr;
extern void	__kernel_entry_point;

//fixme
#define	kernel_assert(e)	((e) ? (void)0 : __kernel_assert(__func__, \
				__FILE__, __LINE__, #e))
void	__kernel_assert(const char *, const char *, int, const char *) __dead2;
void	kernel_panic(const char *fmt, ...) __dead2;
#define printf kernel_printf
int	kernel_printf(const char *fmt, ...);
void	hw_reboot(void) __dead2;
int	kernel_vprintf(const char *fmt, va_list ap);

/*
 * Bootloader routines
 */
void	boot_alloc_init(void);
void	boot_alloc_enable_system(void * ctrl);
void *	boot_alloc(size_t s);
void	boot_free(void * p);

int	boot_printf(const char *fmt, ...);
int	boot_vprintf(const char *fmt, va_list ap);
void	boot_printf_syscall_enable(void);

void *	elf_loader(const char * s, int direct_map, size_t * maxaddr);
void *	elf_loader_mem(void * addr, int len, int direct_map, size_t *maxaddr);
void *	load(const char * filename, int * len);

void	load_kernel(const char * file);
void *	load_module(module_t type, const char * file, int arg);

void	hw_init(void);
void	install_exception_vector(void);

void	caches_invalidate(void * addr, size_t size);

void	glue_memmgt(void * memmgt_ctrl, void* ns_ctrl);

int	acts_alive(boot_elem_t * boot_list, size_t  boot_list_len);

void	stats_init(void);
void	stats_display(void);

#endif
