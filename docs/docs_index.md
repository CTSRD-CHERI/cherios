
### Code organisation

CheriOS has most of its non-system programs in its root directory.
The nanokernel and boot code are in the boot directory.
All OS related programs are in the cherios directory.
The following are important:

* [__boot__](boot.md): boot code. ALSO still contains the init program. If you want to disable/enable programs being loaded during init, see the list in /boot/src/init/init.c
* [__boot/nanokernel__](nanokernel.md): nanokernel code. Provides reservations, foundations, CPU contexts, and wraps access to the architecture for the kernel.


* [__cherios/kernel__](microkernel.md): microkernel. Handles exceptions/interrupts, scheduling, and message-passing between activations.


* __cherios/core/namespace__: provides a directory of registered activations (see /include/namespace.h)
* [__cherios/core/memmgt__](memman.md): provides the system-wide memory map (see /include/sys.mman.h)
* __cherios/system/type_manager__: Handles allocations of sealing capabilities. Manages 'Type Ownership Principles' (TOPs), in the same way the memory manager has MOPs.
* [__cherios/core/proc_manager__](proc.md): provides the process model for c/c++ programs (see include/thread.h)

* __cherios/system/fatfs__: simple FAT filesystem module (see use include/cheristd.h for wrappers)
* __cherios/system/lwip__: Web stack (see include/net.h for wrappers)
* __cherios/system/libsocket__: The socket library. Most processes link against this as a dynamic library for their sockets.
* [__cherios/system/dylink__](dylink.md): The dynamic linker. CheriOS dynamic linking works by dynamic libraries statically building in a dynamic-linking server.

* __libuser__: all modules are linked against it. Provides several libc functions as well as cherios-related functions.
* [__libuser/src/capmalloc/capmalloc_slabs.cpp__](malloc.md) The latest malloc implementation for CheriOS. 
## Demos

## Alice Bob

There is a simple two party [messaging demo](../demos/alice_bob/README.txt) that demonstrates how to use nanokernel primitives for secure messaging between two processes.
