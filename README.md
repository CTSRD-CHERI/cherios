# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI memory protection and object capabilities.

This is Lawrence Esswood's branch, it has now substantially diverged from the original cherios and is very much work in progress. The major difference is the addition of a security hypervisor, the nano kernel. You can read about it elsewhere. It currently has: Virtual memory, multicore support, a file system, a network stack and a webserver. The nanokernel also offers some userspace primitives for integrity, confidentially, and attestation. The aim of this is to provide isolation between mutually distrusting compartments.

### Building CheriOS

You need a Cheri SDK to build CheriOS.

The easiest way to get a CheriOS to work is by using [cheribuild], which will automically fetch all dependencies:
```sh
$ cheribuild.py cherios
```
This will, by default, build for QEMU 128, single core, without networking. Use the --cherios/smp-cores=X option to control the number of cores (1 and 2 have both been tested, but more should work). Use --cherios/build-net to enable networking.

By default this will checkout all the projects to `$HOME/cheri` but this can be changed with `--source-root` or by using a JSON config file (`echo '{ "source-root": "/foo/bar" }' > ~/.config/cheribuild.json`). For more details see [the cheribuild README](https://github.com/CTSRD-CHERI/cheribuild/blob/master/README.md).

### Running CheriOS

CheriOS can be run on QEMU or FPGA.

The following snipset shows how to use CheriBuild to run CheriOS on [cheri-qemu]:
```sh
$ cheribuild.py run-cherios
```

The command line options are currently based on the default cherios target.

### Running CheriOS with networking

Build and run cherios with networking:

```sh
$ cheribuild.py cherios --cherios/build-net
$ cheribuild.py run-cherios --cherios/build-net
```

You should already have a tap device set up called cherios_tap before running QEMU. Configuration for the GUEST can be found in cherios/system/lwip/include/hostconfig.h. You should select appropriate values.

### Code organisation

CheriOS has most of its programs at the top level. The nano kernel and boot code are in the boot directory. All OS related things are in the cherios directory. You probably only care about the following:

* __boot__: boot code. ALSO still contains the init code. If you want to disable things see the list in /boot/src/init/init.c
* __boot/nanokernel__: nano kernel code

* __cherios/kernel__: kernel

* __cherios/core/namespace__: provides a directory of registered activations (see /include/namespace.h)
* __cherios/core/memmgt__: provides the system-wide mmap (see /include/sys.mman.h)
* __cherios/core/proc_manager__: provides the process model for c/c++ programs (see include/thread.h)
* __cherios/system/fatfs__: simple FAT filesystem module (see use include/unistd.h for wrappers)
* __cherios/system/lwip__: Web stack (see include/net.h for wrappers)


* __libuser__: all modules are linked againt it. Provides several libc function as well as cherios-related functions


### Writing a program

There is handy template folder at the top level which you can copy and paste to create a new program. You will then need to add add_subdirectory(my_folder_name) to the top level CMakeLists.txt. This will generate an elf called my_folder_name.elf and also add it to the static file system. To run it during init, add an B_DENTRY(m_user, "my_folder_name.elf", 0,	1) entry to the list in /boot/src/init/init.c. 

You can also copy and paste the secure_template, which automatically gets loaded into a foundation, uses temporally safe stacks, and distrusting calls by default. You will need to use m_secure rather than m_user in the list in init.

### Getting reservations in userspace

Reservations are a very heavily used feature, and many interfaces require a res_t. Get them using cap_malloc. Malloc is wrapper for cap_malloc that will convert the reservation to a memory capability. As well as malloc and free, there is now also a claim function. See /include/sys.mman.h about claiming. 

### Notes

CheriOS-microkernel is still in a early state. A crashing propogram will print a backtrace and then panic the kernel. There is currently no dynamic-linker, all dynamic linker is performed by hand written code.

### Tips and tricks

There is a HW_TRACE_ON and HW_TRACE_OFF macro to turn instruction level tracing on/off. If a section of code faults, it can be useful to surround with these macros. TRACE_ON / TRACE_OFF does a similar thing in assembly.

As debugging tools are limited, it is often useful to break all sandboxing. The obtain_super_powers nanokernel function will unbound the program counter to the entire address space, allow access to system registers, and also return a read/write capabilities to the entire address sace.

If you want the kernel to go faster you can turn off a lot of debugging, or enable a LITE version in cherios/kernel/include/kernel.h. 



   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm-project>
   [cheribuild]: <https://github.com/CTSRD-CHERI/cheribuild>
