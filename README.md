# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI memory protection and object capabilities.

### Note

CheriOS-microkernel is still in a very early state.

 * The code is not well documented
 * It lacks several security checks/actions (thus the model is not secure yet)
 * Expect races, non-recovery on error, ...

### Building CheriOS

You need a Cheri SDK ([LLVM] and [Clang]) to build CheriOS.

256-bit Cheri SDK works out of the box.
128-bit Cheri SDK is not officially supported yet (it works with minor tweaks).

The following snipset will build CheriOS for a 256-bit SDK targetting [cheri-qemu] (defaults).
```sh
$ git clone https://github.com/CTSRD-CHERI/cherios.git cherios
$ cd cherios
$ ./build.sh
```

### Running CheriOS

CheriOS can run on:

 * [cheri-qemu]
 * the CHERI fpga model
 * the CHERI l3 simulator

The target can be choosen by setting xxx in `CMakelists.txt`

The following snipset shows how to run CheriOS on [cheri-qemu]:
```sh
$ dd if=/dev/zero of=disk.img bs=1M count=1
$ qemu-system-cheri -M malta -kernel cherios.elf -nographic -no-reboot -m 2048 \
   -drive if=none,file=disk.img,id=drv,format=raw -device virtio-blk-device,drive=drv
```

### Code organisation

CheriOS code is organized as follow:

* __kernel__: kernel (the interesting part)
* __boot__: boot code
* __fatfs__: simple FAT filesystem module
* __hello__: Hello World module
* __include__: generic includes used by several modules
* __ldscripts__: link scripts user by modules
* __libuser__: all modules are linked againt it. Provides several libc function as well as cherios-related functions
* __memmgt__: provides the system-wide mmap
* __namespace__: provides a directory of registered activations
* __prga__: test program
* __socket__: module providing a minimalistic implementation of sockets
* __uart__: module providing print services
* __virtio-blk__: VirtIO over MMIO module
* __zlib__: zlib module
* __zlib_test__: test/benchmark for the zlib module


   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm>
   [Clang]: <https://github.com/CTSRD-CHERI/clang>


