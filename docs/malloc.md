# Malloc

Malloc on CheriOS deserves some explanation.
On CheriOS, objects can be shared _between_ processes.
However, these processes need not trust each other, and are free to have their own malloc implementations.

Internally, malloc works using _reservations_.
Reservations are a nanokernel abstraction, see __nano_if_list.h__

Malloc on CheriOS has as a `claim` function as well as `malloc` and `free`.
Allocations are manually reference counted using them.
`malloc` and `claim` increases the reference count by one, and free decreases it.
This is backwards compatible, as a program not written for CheriOS will never call `free`. 

Allocations made by the default allocator are memory safe (both spatially and temporally).
If an access succeeds (i.e. does not fault), it will always be to the intended, uncorrupted object, even if the use is technically a use-after-free.

## Reservation vs memory capability

A reservation (`res_t`) is a wrapper for a memory capability (`void *`, or sometimes `capability` to be more obvious).
A reservation can be 'taken' to unwrap it.
Taking a reservation is atomic, and can only be done once.
The nanokernel also gives guarantees about the memory capability resulting from taking a reservation.
Unless revoked:

* It is unique.
* The memory it grants access to does not overlap or alias with any other.
* No other memory capability will ever be granted that overlaps or aliases with it.
* The memory it points to is initially all zero.

Memory accessible from taking a reservation is completely private, unless the capability is explicitly shared.
This is true even if memory management is malicious.
Note that there is no nanokernel guarantee that access will not fault, nor that revocation will not occur prematurely, causing faults on use. 

Many APIs require callers to allocate storage for callee's.
It is suggested that these APIs be updated to use reservations.
This allows the onus for memory allocation to be passed to an untrusted party.
This kind of allocation shift is done so that users have to allocate memory for the kernel, even though they cannot access it themselves.

## Interface

See __capmalloc.h__.
`cap_malloc` works similarly to malloc, but returns a `res_t`.
Because cap_malloc does not get access to the memory capability itself, it need not be trusted (it can be put in another compartment).
malloc itself does, but is in a header and as is much simpler to debug (it is only 9 lines of C).

For the purpose of free-ing, the `res_t` returned by `cap_malloc` is considered two allocations (data and metadata).
Calling `cap_free` on the `res_t` will free both.
Capping `cap_free` on a `void*` that resulted from taking the reservation will free just the data.
The `cap_free_handle` call will free just the metadata.

See __stdlib.h__ for how this is wrapped to provide `malloc`.
It calls `cap_malloc`, takes the reservation, then calls `cap_free_handle`.

Finally, there is `cap_claim` and `claim`.
The distinction between the two is the same as between `malloc` and `cap_malloc`, and changes whether the argument is a reservation or the memory capability that resulted from taking a reservation.
When malloc is called, the resulting object has one 'claim'.
Calling claim will increase this count by one.
Calling free will decrease it by one.
A malloc implementation should ensure that, as long as there is an object in a page with at least one claim,
it has a claim for that page with the memory manager.

Claim allows objects to be transferred between malloc domains.
Malloc domains are generally a process.
Although, conceivably, there may be more than one in a process as well.

An example use, where this function is reached using a message send from another process:

    void send_object(foo_t* ob) {
        claim(ob);
        do_something(ob);
        free(ob);
    }

The reason malloc has a count rather than a single bit is to not have programs be confused into accidentally freeing their own objects prematurely.

Consider a program with the `recieve_object` function also had these functions:

    foo_t make_object(void) {
        return malloc(sizeof(foo_t));
    }
    
    void destroy_object(foo_t* foo) {
        free(foo);
    }

If an adversary called `send_object(make_object())`, and claims were not counters, then `send_object` would not realise the object it was being sent was its own, and might accidentally free it.
Objects that never leave a process will likely never have claim called on them.

## Alternatives to claim

Claim requires not just a call into the compartment handling malloc, but possibly a message send to the memory manager.
This is fine if an object is actually being transferred or shared for a length of time.
However, there are some other mechanisms if an object is just being "borrowed" for a short duration.

### VMEM_SAFE_DEREFERENCE

Semantically, `VMEM_SAFE_DEREFERENCE(result, location, default)` performs the operation
`result = Location_is_mapped(location) ? *location : default`.
This is achieved using the following sequence:

    result = default
    result = *location
    magic_nop();
    
In the case that the access succeeds, result will be set to location.
If the second instruction faults, the magic NOP is an indication to the trap handler that the instruction should be skipped.

in the case where there is no attack, it takes only a few (generally 2) more instructions than a simple load.

A few example uses:

* Setting a variable to notify another thread
* Taking / releasing a lock

VMEM_SAFE_DEREFERENCE also sees use for a programs _own_ allocations as well.
Consider the following (single-threaded) interface provided (using message sends) to an untrusted adversary some kind of session:

    session_t new_session(void) { 
        return seal(malloc(sizeof(session_t));
    }

    void do_something_with_session(session_t session) {
        session = unseal(session);
        something(session);
    }

    void finish_session(session_t session) {
        session = unseal(session);
        free(session);
    }

This interface seals the object it returns (stopping modification by the caller), and does its own malloc and free, in a seemingly sensible way.
However, there is nothing to stop an adversary from calling `do_something_with_session` on an already finished session.
Here is the code again but with a fix:

    session_t new_session(void) { 
        return seal(malloc(sizeof(session_t));
    }

    session_t check_session(session_t session) {
        session = unseal(session);
        state s;
        VMEM_SAFE_DEREFERENCE(s, &session->state, finished);
        return s == finished ? NULL : session;
    }

    void do_something_with_session(session_t session) {
        session = check_session(session);
        if (!session)
            return;
        something(session);
    }

    void finish_session(session_t session) {
        session = check_session(session);
        if (!session)
            return;
        session->state = finished;
        free(session);
    }

Now, even if unmapped, check_session will note that the session has been finished with.
After check_session has been called, further use of VMEM_SAFE_DEREFERENCE is not needed, as the fact that state was not set to finished means the object will not have been freed.
There is no way to use this interface to trigger a use-after-free bug.

### Exceptions

CheriOS has a mechanism of user-exception delivery.
See __exceptions.c__
It was intended that large objects (such as buffers used in the socket library) that required temporary access would just be surrounded with a try-catch like mechanism.

Although CheriOS can deliver exceptions, cross-compartment unwinding still needs implementing.
Because a single executive cannot unwind the stack (because cross-compartment returns are sealed), this requires an unwinder in every compartment to work together.

### Don't bother

If two programs trust each other that one is happy to crash if a contract is broken,
there is no actual _need_ to call claim at all if the program that performed the allocation gives the guarantee it will not free it.

## Missing claims

No good excuse here, apart from claim not existing at some points during development.
Claims are missing in some parts of the codebase.
This is not so much a security issue, but a stability one.
Programs should not be able to cause other important programs (such as the kernel) to crash, but calls to claim are spotty in places.

The socket library is pretty good at wrapping access to locks using VMEM_SAFE_DEREFERENCE, but should be using exception handling for buffers themselves, but is not.
An adversary could, for example, bring down the TCP stack because of this.

## Implementation

The current (and best?) malloc implementation so far is in __libuser/src/capmalloc/capmalloc_slabs.cpp__.
__capmalloc_bump.c__ is flawed and not as efficient.

It is two layers:
* An allocation layer
* A tracking layer

The allocation layer uses the tracking layer every time malloc is called.
The tracking layer tracks where claims are, even if they were allocation not by itself.

### Allocation

capmalloc_slabs allocates objects in two ways.
The first is just pushing all the work on to the memory manager.
If the object is larger than `BIG_OBJECT_THRESHOLD`, a request is just made to the memory manager.

If not, then a slab allocator is used.
A slab allocator has slabs of object, where each slab has objects of the same size.
In order to minimise reservation metadata overhead, the nanokernel `split_sub` call is used to only create one metadata node for up to 64 objects.
Because of this, the sizes supported are those supported by the nanokernel.

### Tracking

Each malloc implementation currently maintains its own three-tired page-table like structure to reference count at a page-granularity.
This avoids over stressing the memory manager, and makes malloc faster.
Malloc only informs the memory manager the count for a page changes to/from 0.
As future work, we might want a fast-path for objects that will not be shared (i.e. don't have reference counting for them).

### Revocation.

Reservations cannot be re-used after they have been taken.
Because of this, a single object might cause a page to stay resident, which is very inefficient.
Supporting fine-grained revocation is a todo, but would require a more complicated interface (likely shared memory) interface between the memory manager and malloc implementations.

## FAQ

Nobody asked, but I suspect they might.

* What happens if I access an object that has been freed - either a page-fault will be generated, or an untagged exception will be generated, or the access will go ahead _as if you had never freed the object_.
* What happens when I use a capability that has been revoked - you will get a tag exception if you dereference it. _Comparisons may still compare equal_. Recently, the CHERI compiler has shifted to using non-exact equality (just comparing address) for the '==' operator. Make sure you use the exact equal comparison if you are worried.
* What happens if I claim an object coming from a vulnerable / malicious malloc - as long as the malloc you use is fine, there is nothing other implementations can do to interfere with your usage.
* What happens if the memory manager is vulnerable / malicious - you may get an exception even though you did not free the object. Memory safety is still guaranteed.
* What happens if I use my own malloc - all bets are off.
