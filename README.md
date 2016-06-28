# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI memory protection and object capabilities.

### Note

CheriOS-microkernel is still in a very early state.

 * The code is not well documented
 * It lacks several security checks/actions (thus the model is not secure yet)
 * Expect races, non-recovery on error, ...

### Installation

You need a 256-bits Cheri SDK ([LLVM] and [Clang]) to build CheriOS and [cheri-qemu] to run it.

```sh
$ git clone https://github.com/CTSRD-CHERI/cherios.git cherios
$ cd cherios
$ ./build.sh
$ qemu-system-cheri -M malta -kernel cherios.elf -nographic -no-reboot -m 2048
```

### Code organisation

CheriOS code is organized as follow:

* __include__: generic includes used by several modules
* __kernel__: kernel and boot code
* __ldscripts__: link scripts user by modules
* __libuser__: all modules are linked againt it. Provides several libc function as well as cherios-related functions
* __memmgt__: provides the system-wide mmap
* __namespace__: provides a directory of registered activations
* __prga__: test program
* __socket__: module providing a minimalistic implementation of sockets
* __uart__: module providing print services


   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm>
   [Clang]: <https://github.com/CTSRD-CHERI/clang>


