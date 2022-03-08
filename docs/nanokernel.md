## The nanokernel

The nanokernel is a security focussed abstraction layer.
Its main goal is to sub-divide overly-permissive hardware capabilities, enforce some restricitons on hardware management, and provide capability primitives that can be used to attest to the security of system state.
It is best described in the [CheriOS technical report](https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-961.pdf).
It is intended to be a thin-as-possible wrapper to the underlying architecture, and so some parts of its interface are platform dependant.

It is still the role of the microkernel to provide robustness.
It is fine for misuse of more privileged nanokernel calls (such as those that modify CSRs) to break the system, as long as the nanokernel's security goals would not be broken.

Some nano calls are also intended for direct use by user programs as well.
These are more robust, and should always be safe to call, although the calling context may still break itself.

### Interface

It is intended that nanokernel be reached in the same way a dynamic-library would be.
It has no call stack so for the most part all functions are leaf functions.
Although not a C program, the nanokernel obeys the C ABI with respect to calling conventions.

The interface for the nanokernel is described in __nano_if_list.h__.
Some hand-written glue code (see __cheriplt.h__) takes a `nano_kernel_if_t` (a table of nanokernel function pointers) and uses it to link the nanokernel.

Although eventually these nanokernel functions are reached using the CHERI ccall/cinvoke instructions, there is another interface to populate the `nano_kernel_if_t` in the first place.
To stop the microkernel substituting its own capabilities for nanokernel capabilities (we explictly _dont_ want to support nanokernel virtulisation) the nanokernel can be reached with a tranditional syscall to request its interface.

However, not every user process should have access to every nanokernel function.
In order to request the interface, a `if_req_auth_t` object is required.
A `if_req_auth_t` is provided to users by the microkernel, and can be made more restrictive using the `if_req_and_mask` nano call.
Currently, no effort has been made to actually give the correct nanokernel functions to the correct programs.
This obviously needs correcting at some point.
