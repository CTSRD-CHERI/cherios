# Dynamic linking

By default, the dynamic library is the default boundary of protection.
To a user process, both the microkernel and nanokernel are dynamic libraries.

Dynamic linking is a little more complicated than on other systems, as dynamic libraries are distrusting of others, including the microkernel and process manager.
There is therefore no single agent that can be trusted to link together programs.
Instead, in the general case, dynamic libraries expose a 'link-server', a separate thread of execution within a library that has the authority to release capabilities to parts of the library.
This is achieved my making dynamic libraries standalone executables, that when run will expose a server to allow new instances of processes that use the library. 

## Calling conventions

Cross library calls are achieved by calling PLT (Procedure-linkage-table) stubs.
Each stub represents a function that will be in another library.
Because there can be quite a lot of work to do for a cross-library call,
CheriOS PLT stubs just load pointers for the target function, then jump to a helper.
These helpers are in __stubs.S__.

We will always describe calling conventions from the point of view of the caller.
So if the caller does not trust the callee, this is an _untrusting_ call,
and if the callee does not trust the caller, it is an _untrusted_ call.
Calls can be any combination of trusting/untrusting and trusted/untrusted.

There are several caller side modes.
`plt_common_single_domain` is only really used to call into the nanokernel.
`plt_common_complete_trusting` is used when the caller trusts all other libraries.
`plt_common_trusting` is used the caller trusts the callee, but not all libraries.
`plt_common_untrusting` is used when the caller does not trust the callee.

On the callee side, each function has three entry points (these are created by the compiler).
The first entry point is used for within library calls.
The second first loads a stack and globals pointer, and then falls through to the first.
Use `CROSS_DOMAIN(X)` to get the cross-domain symbol for a function.
This is for trusted callers.
The last entry calls a helper (`entry_stub` in stubs.S), and then falls through to the second.
Use `UNTRUSTED_CROSS_DOMAIN(X)` to get the untrusted cross-domain symbol for a function.
This is for untrusted callers.

The calling convention for CheriOS is that callers/callees _should_ save and restore registers as defined by the ABI for the platform.
The untrusting/untrusted helpers will assume the ABI is not being obeyed, do extra saving and restoring of registers, and enforce correct control flow between library boundaries.
Untrusting calls will also protect the saved state of the caller by sealing capabilities before making the call.
The result is, even if the other party is unconstrained assembly, only explicitly passed arguments are accessible.
The callee can choose to never return, but cannot access the callers state, or interfere with the callers control flow.

## Methods of linking

Both for reasons of bootstrapping and simplicity, there are a few ways dynamic-linking is performed.

### Simple and manual

The simplest kind of dynamic linking is that used to link with the microkernel and nanokernel.
It requires quite a lot of boilerplate, but has a simple mechanism, which is useful as no syscalls can be performed before linking the microkernel,
and more complex mechanisms require such calls.
The microkernel and nanokernel require no symbols from the user, and so just specify their interfaces with a struct of function pointers.

The interface for the microkernel is in __syscalls.h__ and __nano_if_list.h__.
These macro lists are processed to define the struct that contains (sealed) function pointers, autogenerate PLT stubs, and generates a function that takes the struct and populates captable entries used by PLT stubs.
The macro lists are also processed to generate the table in the first place.
See __cheriplt.h__ for auto-generation.
Linking is called in `object_init` in __object.c__.

You can add to these lists, but should not use this method of linking for anything else.

### General but manual (deprecated)

The more general way of linking requires a link-server.
This should be done for you by the compiler, but there is a manual way of doing it using __cherios/system/dylink/include/manual_dylink_server.c__
Only the socket library currently uses it, and new programs should just use the fully general method.

The symbols to be exported still need to be defined manually in a macro-list.
The socket library has its list in __cherios/system/libsocket/include/socket_common.h__
Calling `server_start` will build a table of function pointers, sign it, and then executes an event loop that can handle three different requests:
* `get_size_for_new` - will return the memory required for a new process
* `get_if_cert` - will give a certificate with the table of exported functions.
* `create_new_external_thread` - will create a new thread _for another process_ (not the link-server)

Linking with this library happens automically, again in `object_init` in __object.c__.

### Fully General

The fully general method of dynamic linking creates and exchanges data structures based on ELF headers and relocations.
A dynamic library should include __server.c__.
This is done automatically using the `add_cherios_library` cmake function (which links a static library containing everything required for a link sever).
This will give the library a `main`, so it can also be loaded as a statically-linked program.
This program is not the library itself, but the link server for the library.

There is some weirdness here as to how symbols will be resolved.
In order to operate, the link-server needs some functions to be defined (e.g., memcpy and malloc) for its own use.
There are two ways a function might be invoked: as the link-server, or as an instance of the dynamic library itself.
If the link-server executes a function, it will always be one statically linked into the server (and so can be trusted to behave as expected).
However, when executed as the dynamic library, functions may end up being provided by another library.
Note the `DEFAULT_IMPL` definitions in __client.c__, which create weak symbols for a few functions.

As an example, `cap_malloc` will likely be provided by the main program.
This means if (during linking, or later), code in __server.c__ or __client.c__ calls cap_malloc, the result may be from an untrusted party.
Note that (in this case) this not a security issue, as the result of cap_malloc is a reservation.
The code to convert it to a memory capability is in __stdlib.h__, and is a static function, so cannot be overridden.
If the security of a link-server depends on the implementation of a function, make sure it is either static, or its results verifiable.

On startup, the link-server will register with the namespace manager, so it can be found by clients.
It only offers two methods that can called via message sends:
* get_requirements - gets a (possibly signed) description of the requirements of the dynamic library.
* new_process - will create a new process that uses the library.
These functions are executed _as the link server_.

Programs that require dynamic libraries should include __client.c__
`auto_dylink_new_process` will lookup link-servers (it is solely the choice of the main program to select its libraries, although they can choose to reject each other) and starts a session that combines them.
It then provides this session to every link-server using `new_process`.
Once `new_process` has been called (via message send), functions in client.c can be called directly in _other_ libraries (_not as the link server_) by the main program.
Because no symbols have actually been exchanged yet, the `CALL_PARTNER_FUNC` macro in __client.c__ is used to invoke a function in another library.
Function pointers were explicitly shared for this purpose when `new_process` was called.

Each library dynamically allocates its own PLT stubs.
The next stage in linking is to exchange symbols.
Every server also acts a client in this step.
Each client (starting with the main program) goes through its relocations (in batches, see `batch_symbols`) and calls into other libraries to see if they have symbols with that name.
Note that each library communicates directly with all others.
This means it is possible for libraries to only share symbols with 'friendly' libraries, or to select different trust modes for calls.

Currently, symbols are shared based only on their visibility attributes.
It is the goal CheriOS gets to a point where libraries begin to verify each other using the foundation identity mechanism.
Libraries should refuse to share symbols in certain cases, but should not ever return different symbols in a way that would break the C requirement for pointers to the same object to compare equal.
Giving an error is likely better.

## Problems with function pointers

Cross-library calls require two pointers: a capability to code and a data capability used together to execute a CHERI ccall/cinvoke instruction.
However, in C, a function pointer needs to be a single pointer, not a pair.
Currently, function pointers passed across library boundaries do not work properly.
Compiler work is required to turn function pointers into pointers to a pair, and to compare/invoke them properly.
PLT helpers know how to invoke pairs, so they are functional.

Use `CROSS_DOMAIN(X)` and `UNTRUSTED_CROSS_DOMAIN(X)` to get function pointers for cross-domain calls.
Use `TRUSTED_DATA` and `UNTRUSTED_DATA` to get the second argument.
You can manually use `INVOKE_FUNCTION_POINTER` in __dylink.h__ to call such a pair.
