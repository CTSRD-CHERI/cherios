# The microkernel

The CheriOS microkernel is in the __cherios/kernel__ directory.

It handles interrupts, exceptions, scheduling and message passing.
Scheduling and message-queues are tied together into a single abstraction (the object activation), see `act_t` in __activations.h__. 

## Exceptions and interrupts

The nanokernel provides a wrapper around handling interrupts and exceptions.
Rather than just a change to the program counter, exceptions are handled by switching every register to a pre-registered CPU context.
Different platforms might have different contexts for different exceptions (to mirror what would be offered by vectored exceptions)

The nanokernel call to register a context for exceptions (on MIPS at least) is

`void set_exception_handler(context_t context, register_t cpu_id)`

Exceptions are then handled as a main loop in __cherios/kernel/src/platform/X/kernel_exceptions.c__

This loop will read the state of the exception-handling registers using the nanokernel's `get_last_exception` call.
Contents of the struct are platform dependant.
It will then take some action, and then call `context_switch` to either restore the context that faulted, or another.


### User exceptions

Because the state of a user program is sealed away into the a CPU context, it is difficult to deliver user exceptions using the microkernel.

Instead, there is a low level nanokernel ABI that allows a CPU context to handle its own exceptions.
See the `exception_X` family of nanokernel functions.
If the microkernel does not want this delivery, the capabilities can be denied to the user.
Briefly, a user can call subscribe to handle its own exceptions, and then can either replay them (if they cannot handle the fault), or return (which will re-attempt the operation)

## Scheduling

__cherios/kernel/src/sched.c__ contains a simple scheduler.

It contains a number of queues, one for each priority level.
See `enum sched_prio` for the levels.
Each level of priority is given SCHED_PRIO_FACTOR times as many slots as the next.
Within a priority level, a round-robin scheme is used.

## Messaging

__cherios/kernel/src/msg.c__ contains the kernel's message queue logic.
It is a variable-(but power of two)-sized multi-reader single-reader fifo, implemented as a ring-buffer.

Message are _enqueued_ by the microkernel (which handles any write races), but are _dequeued_ in userspace.
See __libuser/src/msg.c__ and __libuser__/src/platform/msg.S__ for the corresponding userspace code.

Messages can also be passed directly between activations without ever touching a queue, as an optional fastpath.
This requires the receiver to already be waiting, and the caller to use a special fathpath call.
This is handled by the microkernel in assembly, see __cherios/kernel/src/platform/message_send.S__

## System calls.

User processes link with the microkernel as if it were a dynamic library.
See __syscall.h__.
Like the nanokernel's `nano_kernel_if_t`, there is a `kernel_if_t` which is table of all the microkernel's system calls.
This is provided by the microkernel to every new activation.
Unlike the nanokernel, functions can be swapped out in this table to interpose system calls, or disallow them by setting them to NULL.
