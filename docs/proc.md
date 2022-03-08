## Process Management

The process manager (__cherios/core/proc_manager__) creates an abstraction to support a process model.
A process is a collection of threads, each with a private store, and a common memory pool with the lifetime of the process.

### Threads

The process manager creates a new activation for every thread a process tries to create.
If the process is 'insecurely' loaded, the process manager handles allocation of thread local storage.
Otherwise, the constructing thread must create storage for the new thread.

### Loading ELFs

Because all CheriOS processes live in the same address space, it is not possible to give ELF objects an absolute address.
The process manager will lay out ELF objects respecting program headers up to alignment and memsize, but segments may not actually be relative to each other as described.
For example, if the following header were provided (notice the two segments are not contiguous):

    Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
    LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x010000 0x010000 R E 0x10
    LOAD           0x020000 0x0000000000020000 0x0000000000020000 0x010000 0x010000 RW  0x10

Then the ELF loader will create two 0x010000 sized allocations, likely next to each other, to avoid big holes.
There is therefore a more complicated mapping between a logical address and a capability for a symbol with that address than just adding
an offset to a 'base' capability.
All capability relocations on CheriOS are always _segment relative_, see __crt.h__.
This makes handling them easy.
Other dynamic relocations are not (yet) segment relative, and so a more complicated lookup is required.
see `crt_logical_to_cap` in __crtbeginC.c__ for this lookup.

When a process is started, it is passed a table of capabilities for each of its segments, and can process its own relocations.
Relocations that target the TLS segment are also re-processed every time a thread is started.
See __init_asm/platform/X/init.S__.

The process manager will create a new MOP for each process it creates, and use it to allocate space for a processes' segments on the processes' behalf.
Every new thread for a process gets given the same table, apart from a different capability for its TLS segment.

This method of loading is considered the 'insecure' way of loading a process.

### Secure Loading

Secure process loading is similar to insecure in the first few steps.
The process manager will first load the ELF object (not respecting offsets between segments) into memory.

However, rather than this being the actual set of segments for the process, it is used as a prototype to be copied.
The nanokernel `foundation_create` call will copy what the ELF loader has created into a region that the ELF loader does not have access to.
As it does so it takes a SHA256 hash, which will constitute part of the identity of the foundation.
The new activation created for the process will be setup as to immediately call `foundation_enter`.

The entry point within a secure loaded program is in __init_asm/platform/X/secure_init.S__.
This will reprocess the ELF header to build a table of segments, to then jump to the normal entry.

Secure loaded programs also get given their own type ownership principle (TOP).
The process manager acquires a sealing capability reservation on behalf of the process to pass to it on startup, so the process can seal its own objects.

Because the exact layout of a process would change its SHA256 hash, the process loader promises to do it in a specific way.
All segments will be placed sequentially, _in the order they appear in the ELF program header_ with minimal padding as to respect alignment.
If there is a TLS segment present, _another_ segment is generated at the end, full of zeros, with the memory size of the TLS segment.
This is to act as the TLS segment for the first thread.
Subsequent TLS segments are allocated by the thread creating the new thread, not the process manager.
See __generate_found_id.py__ for a python script that generates the expected hash given an ELF.
