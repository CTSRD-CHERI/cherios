## Booting

CheriOS has a few stages in booting.

### Pre-nanokernel

It is intended that if a CheriOS-style system were to ever ship, the nanokernel would be included as a part of firmware,
which would be the first entry point and would lock down the system before moving on to the rest of a trusted boot sequence.

Currently, there is a single pre-nanokernel ELF that does some bootstrapping to load the nanokernel, microkernel, and init elf objects into memory.
It is contained in __boot/src/boot__.
After it has placed these three objects in memory, it will call the entry nanokernel entry point.
The intention is to get rid of this step, as it runs with overly permissive authority, and before the nanokernel has secured the system.

### Nanokernel

The nanokernel is in __boot/nanokernel__

The nanokernel entry point is a function with the C prototype

`nano_kernel_init(register_t unmanaged_space, register_t entry_offset, packaged args)`

Although the pre-nanokernel boot was a C environment, the nanokernel is not.
`nanokernel_init` will never return, and also clears registers such as the stack register as part of securing the system.
Note that, even though the pre-nanokernel loaded ELF objects for both the init process and the microkernel, there is no mention of this in the `nano_kernel_init` function.

The nanokernel does not view the system as a collection of processes, nor does it have the notion of a microkernel.
It thinks of the system in two halves: private nanokernel memory and everything else.
'Everything else' might be an operating system with processes, or just a single baremetal program.

`unmanaged_space` is how much _physical_ memory should be left outside of the control of the nanokernel.
In our case, it is the CheriOS microkernel and init processes ELF objects.

`entry_offset` is an offset into _physical_ memory that the nanokernel should transfer control to after it has initialised itself.
It will be called with capabilities to manage the unmanaged space, and capabilities to call nanokernel routines.

`args` are arguments that are passed through nanokernel init. No capabilities can be passed.

### Microkernel

The CheriOS microkernel is in __cherios/kernel__

Its entry point is `cherios_main`.

It will initialise activation management, the scheduler, and exception and interrupt handling.
`cherios_main` is passed the location in memory where the init elf object is, and will use it to create the first activation,
which it will then schedule.

### Init

Init is in __boot/init__

Init is not a full process.
A CheriOS process is a combination of a number of activations (managed by the microkernel) and a memory ownership principle (managed by the memory manager), all tracked by the process manager which creates them, and will ensure their lifetimes end together.
However, the process manager and the memory manager have not yet been loaded, and so there is no such thing as a process.

__boot/init/init.c__ has a list of ELF objects it will start.
Currently, these objects are just embedded in it.
It is intended that only some core programs be embedded in this way, and subsequent programs loaded from a filesystem (although nothing in CheriOS currently loads programs this way).
The first few programs will not actually be launched as processes (just standalone activations), as processes still require the process manager.

Before the memory manager is loaded, everything is physically mapped.
All programs are directly mapped into the space not managed by the nanokernel.
The allocator for this space is described in __tmpalloc/include/tmpalloc.h__.
Init uses this pool to load the process manager, then passes the rest of it to the process manager to use before the memory manager is loaded.

Once the namespace, process and memory managers are brought up, programs will be loaded as processes and in virtual memory.
At some point, it would be nice to have a re-loading stage, where some of the early loaded programs are re-loaded as full processes, once such a facility is available. 

Before the libsocket and uart programs are loaded, stdout for processes will be provided by microkernel calls (only available in debug mode).
After they are loaded, stdout will be a socket backed by the uart driver.

The type manager is loaded next, as it is required to give secure-loaded processes a sealing capablity.
