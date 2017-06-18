# CheriOS-microkernel

CheriOS is a minimal microkernel that demonstrates "clean-slate" CHERI memory protection and object capabilities.

### Note

CheriOS-microkernel is still in a very early state.

 * The code is not well documented
 * It lacks several security checks/actions (thus the model is not secure yet)
 * Expect races, non-recovery on error, ...

### Building pure-MIPS CheriOS

This is a pure MIPS microkernel, no security, acts as a baseline for evaluation purposes.
MIPS64 clang/llvm required.

The following snipset will build CheriOS for a 256-bit SDK targetting [cheri-qemu] (defaults).
```sh
$ git clone https://github.com/CTSRD-CHERI/cherios.git cherios
$ cd cherios
$ ./build.sh
```

### Running CheriOS

This CheriOS can run on:

 * [cheri-qemu]
 * the BERI fpga model
 * the BERI l3 simulator

The target can be choosen by setting xxx in `CMakelists.txt`

The following snipset shows how to run CheriOS on [cheri-qemu]:
```sh
$ qemu-system-cheri -M malta -kernel cherios.elf -nographic -no-reboot -m 2048
```

### Code organisation

CheriOS code is organized as follow:

* __kernel__: kernel (the interesting part)
* __boot__: boot code
* __include__: generic includes used by several modules
* __ldscripts__: link scripts user by modules
* __libuser__: all modules are linked againt it. Provides several libc function as well as cherios-related functions
* __memmgt__: provides the system-wide mmap
* __namespace__: provides a directory of registered activations
* __socket__: module providing a minimalistic implementation of sockets
* __qsort, AES, stringsearch, dijkstra, spam, sha, CRC32, bitcount, adpcm__: MiBench benchmarks


   [cheri-qemu]: <https://github.com/CTSRD-CHERI/qemu>
   [LLVM]: <http://github.com/CTSRD-CHERI/llvm>
   [Clang]: <https://github.com/CTSRD-CHERI/clang>
