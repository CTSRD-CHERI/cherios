#ifndef _BOOT_H_
#define _BOOT_H_

#include "mips.h"
#include "cdefs.h"
#include "stdio.h"
#include "syscalls.h"

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

extern char	__start_heap;
extern char	__stop_heap;

extern void	__boot_load_virtaddr;
extern void	__kernel_load_virtaddr;
extern void	__kernel_entry_point;

//fixme
#define	kernel_assert(e)	((e) ? (void)0 : __kernel_assert(__func__, \
				__FILE__, __LINE__, #e))
void	__kernel_assert(const char *, const char *, int, const char *) __dead2;
void	kernel_panic(const char *fmt, ...) __dead2 __printflike(1, 2);
#define printf kernel_printf
int	kernel_printf(const char *fmt, ...) __printflike(1, 2);
void	hw_reboot(void) __dead2;
int	kernel_vprintf(const char *fmt, va_list ap);

extern void	kernel_exception_trampoline;
extern void	kernel_exception_trampoline_end;

extern void kernel_ccall_trampoline;
extern void kernel_ccall_trampoline_end;

/*
 * Bootloader routines
 */
void	boot_alloc_init(void);
void	boot_alloc_enable_system(void * ctrl);
void *	boot_alloc(size_t s);
void	boot_free(void * p);

int	boot_printf(const char *fmt, ...) __printflike(1, 2);
int	boot_vprintf(const char *fmt, va_list ap);
void	boot_printf_syscall_enable(void);

void *	elf_loader(const char * s, int direct_map, size_t * maxaddr);
void *	load(const char * filename, int * len);

struct boot_hack_t {
	kernel_if_t* kernel_if_c;
	act_control_kt self_ctrl;
	queue_t* queue;
};

capability load_kernel(const char * file);
void *	load_module(module_t type, const char * file, int arg);

void	hw_init(void);

void	caches_invalidate(void * addr, size_t size);

int	acts_alive(boot_elem_t * boot_list, size_t  boot_list_len);

void	stats_init(void);
void	stats_display(void);

#endif
