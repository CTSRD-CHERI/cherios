# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI memory protection and object capabilities.

This is Lawrence Esswood's branch, it has now substantially diverged from the original cherios and is very much work in progress. The major difference is the addition of a security hypervisor, the nano kernel. You can read about it elsewhere. It currently has: Virtual memory, multicore support, a file system, a network stack and webserver, and integrity, confidentially, and attestation primitives. The aim of this is to provide isolation between mutually distrusting compartments.

### Note

CheriOS-microkernel is still in a very early state.

 * The code is not well documented
 * It lacks several security checks/actions (thus the model is not secure yet)
 * Expect races, non-recovery on error, ...

### Building CheriOS

You need a Cheri SDK ([LLVM] and [Clang]) to build CheriOS.

256-bit Cheri SDK works out of the box.
128-bit Cheri SDK is NOT supported yet. The alignment and size of some structures would break some code. You will hopefully hit static asserts where is happens.

The easiest way to get a CheriOS to work is by using [cheribuild]:
```sh
$ cheribuild.py llvm
$ cheribuild.py qemu
$ cheribuild.py cherios
```

This will probably fail on the first run as you will need to manually switch to the correct branches of all the repos this grabs, and then rebuild. The branches are...

[cherios] - lawrence
[cheri-qemu] - cherios
[LLVM] - temporal
[Clang] - temporal
[LLD] - temporal

By default this will checkout all the projects to `$HOME/cheri` but this can be changed with `--source-root` or by using a JSON config file (`echo '{ "source-root": "/foo/bar" }' > ~/.config/cheribuild.json`). For more details see [the cheribuild README](https://github.com/CTSRD-CHERI/cheribuild/blob/master/README.md).

### Running CheriOS

CheriOS is run only on QEMU

The target can be chosen by setting xxx in `CMakelists.txt`

The following snipset shows how to run CheriOS on [cheri-qemu]:
```sh
$ $QEMU -M malta \
-drive if=none,file=disk.img,id=drv,format=raw  \
-device virtio-blk-device,drive=drv \
-netdev tap,id=tap0,ifname="cherios_tap",script=no,downscript=no \
-device virtio-net-device,netdev=tap0 \
-D intr.log \
-kernel $IMAGE -nographic -no-reboot -m size=2048  \
-smp 2,threads=2

```
You should already have a tap device set up called cherios_tap. A few notes on changing __any__ of these parameters: CheriOS does not enumerate its devices, if you re-order / remove any it will get very confused. Just disable the driver in software if you don't want it. The memory size is hard coded (nanotypes.h). The number of cores is hard coded (mips.h). You probably want to go to /boot/src/init/init.c, find the list of programs, and disable everything after uart.elf. This will get you core functionality. 

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

### Hints

Reservations are a very heavily used feature. Get them using cap_malloc. Malloc is wrapper for cap_malloc that will convert the reservation to a memory capability. As well as malloc and free, there is now also a claim function. See /include/sys.mman.h about claiming. 

To add a new program, you can add it to the STATIC file system with the cmake command add_cherios_executable. Then add an appropriate line in init.c. You could also load a file from the dynamic file system using standard open/read functions, and then use the functions from thread.h directly, but I have not personally tried. There is handy template folder at the top level which you can copy and paste.
   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm>
   [Clang]: <https://github.com/CTSRD-CHERI/clang>
   [cheribuild]: <https://github.com/CTSRD-CHERI/cheribuild>