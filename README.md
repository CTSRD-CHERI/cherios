# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI software enclaves, memory protection and object capabilities.

This is Lawrence Esswood's fork of the original CheriOS microkernel, which was outlined by Robert N.M. Watson and implemented by Hadrien Barral.
It has now substantially diverged from the original CheriOS.
The major difference is an attempt to de-privilege the OS such that processes can exist in mutual distrust with the OS they run on.
CheriOS augments CHERI architectures with a new abstraction, the 'nanokernel'.
The nanokernel offers some userspace primitives for integrity, confidentially, and attestation, which are built upon by CheriOS to provide fine-grained software enclaves.
You can read about [CheriOS] and its nanokernel. It offers far more coverge of how CheriOS works at a high level.
This README contains instructions on building and developing CheriOS

CheriOS has virtual memory, multicore support, a file system, a network stack (LWIP) and a webserver (NGINX).
CheriOS was initially developed on CHERI-MIPS (which is still the most mature).
There is a basic CHERI-RISCV port (see below), and the intention is to deprecate the MIPS version as the RISCV version becomes mature.

### Building CheriOS

You need a Cheri SDK to build CheriOS. 

The easiest way to get a CheriOS to work is by using [cheribuild], which will automically fetch all dependencies if the `-d` flag is provided:

```sh
$ cheribuild.py cherios-mips64 -d
```

This will, by default, build for QEMU, single-core, without networking.
By default, this will checkout all the projects to `$HOME/cheri` but this can be changed with `--source-root` or by using a JSON config file (`echo '{ "source-root": "/foo/bar" }' > ~/.config/cheribuild.json`).
For more details see [the cheribuild README](https://github.com/CTSRD-CHERI/cheribuild/blob/master/README.md).

Use the `--cherios/smp-cores=X` option to control the number of cores (1 and 2 have both been tested, but more should work).

Use `--cherios/build-net` to enable networking.

The `cherios-riscv64` and `run-cherios-riscv64` targets will build and run for RISCV rather than MIPS. See RISCV progress for more.

Building some CheriOS components requires python3 and some extra python modules listed in requirements.txt in the root cherios directory. Run:

```sh
$ pip3 pip3 install -r requirements.txt
```

### Running CheriOS

CheriOS can be run on QEMU or FPGA.

The following snipset shows how to use CheriBuild to run CheriOS on [cheri-qemu]:
```sh
$ cheribuild.py run-cherios-mips64 -d
```

The command line options are currently based on the default cherios target.

Cheribuild will offer to create a file that represents the block device used by QEMU.
You can resize this if desired.
Running CheriOS once will cause CheriOS to format it as a FAT filesystem.
Once this is done, you can mount it on the host as well.
I would suggest _not_ mounting it concurrent to CheriOS operating.

### Developer documentation

Some more developer information can be found in the [docs](docs/docs_index.md) directory.
You can also read more about [CheriOS] in the tech report.


### Running CheriOS with networking

Build and run cherios with networking:

```sh
$ cheribuild.py cherios-mips64 --cherios/build-net
$ cheribuild.py run-cherios-mips64 --cherios/build-net
```

You should already have a tap device set up on the HOST called cherios_tap, before running QEMU.
Configuration for the GUEST can be found in cherios/system/lwip/include/hostconfig.h.
You should select appropriate values so your guest and host are on the same subnet etc.

### Writing a program

There is handy template folder at the top level which you can copy and paste to create a new program.
You will then need to add add_subdirectory(my_folder_name) to the top level CMakeLists.txt.
This will generate an elf called my_folder_name.elf and also add it to the static file system embedded in init.
To run it during init, add an B_DENTRY(m_user, "my_folder_name.elf", 0,	1) entry to the list in /boot/src/init/init.c. 

You can also copy and paste the secure_template, which automatically gets loaded into a foundation, uses temporally safe stacks, and distrusting calls by default.
You will need to use m_secure rather than m_user in the list in init.

### Getting reservations in userspace

Reservations (a type representing the right to 'request' some specific region of memory) are a very heavily used feature in CheriOS, and many interfaces require a res_t.
Get them using cap_malloc, which has the same interface as malloc.
Malloc is wrapper for cap_malloc that will convert the reservation to a memory capability.
As well as malloc and free, there is now also a claim function.
See /include/sys/mman.h about claiming at a page granularity.
Malloc's malloc/claim/free work similarly but at an object granularity.

### Notes

CheriOS-microkernel is still in a early state. It is liable to crash on occasion. Some annoyances you might hit:

* There is currently no distinction as to which programs are crucial to system operation. Some crashing programs will panic the system, but it is possible for an important service to die and leave the system broken.
* Generic dynamic linking supported was added to CheriOS relatively late. However, some important processes (and even the microkernel itself!) are dynamic libraries. Dynamic linking of these older programs is performed by hand-written / macro-generated code. See cheriplt.h for helpful macros. These need migrating.
* The block cache can, but has no trigger to write back. If you want a persistent file store, remove the cache, or manually trigger a writeback by sending the appropriate message to the cache.
* There is no interpreter. If you want input use the filesystem (cheribuild creates a file that represents the block device QEMU uses) and/or TCP.
* CheriOS can take any memory mapped ELF object (passed as a char*) to load a new program. Currently, every ELF object is just bundled into the 'init' ELF, which contains a list of programs to load. Some core-programs might need loading this way, but others should be migrated to be loaded from the filesystem.

### RISCV Progress

The RISCV port requires a [different branch] of the llvm compiler in order to work.

* There are many `TODO RISCV` comments scattered about that need to be addressed.
* Basic porting of the nanokernel, microkernel, and core services is done.
* Multicore probably will not work.
* Timer interrupts work, but no others do.
* The only driver that is tested and working is the uart_16550.
* Manual dynamic linking works. Generic does not. Changes to server.c and client.c will be required for general dynamic linking to work.
* Secure loading of programs with foundations is functional (But the SHA256 has not been tested).
* Safe stacks have been disabled. The CheriOS RISCV LLVM branch will insert the extra prolog/epilog code for safe stacks, but CheriOS does nothing with them. Getting safe stacks working would require changes in temporal.c
* User exception delivery is not implemented (in the nanokernel and userspace)
* Revocation is not implemented (in the nanokernel)
* Proper backtracing does not work.

### Tips and tricks

There is a HW_TRACE_ON and HW_TRACE_OFF macro to turn instruction level tracing on/off.
If a section of code faults, it can be useful to surround with these macros.
TRACE_ON / TRACE_OFF does a similar thing in assembly.

As debugging tools are limited, it is often useful to break all sandboxing.
The obtain_super_powers nanokernel function will unbound the program counter to the entire address space, allow access to system registers, and also return a read/write capabilities to the entire address space.
It is not intended a system would ever be released with such a facility.

If you want the kernel to go faster you can turn off a lot of debugging, or enable a LITE version in cherios/kernel/include/kernel.h.

   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm-project>
   [cheribuild]: <https://github.com/CTSRD-CHERI/cheribuild>
   [CheriOS]: <https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-961.pdf>
   [different branch]: <https://github.com/CTSRD-CHERI/llvm-project/tree/cherios_riscv>