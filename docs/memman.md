## Memory manager

CheriOS memory management is done by a separate process.
It is contained in __cherios/core/memmgt__.
The role of the memory manager is to manage the mappings between physical and virtual memory, track resource limits,
and organise and batch revocation.


### Memory ownership principals

The memory manager tracks its own memory ownership principles (MOPs).
A MOP does not nessecerily correspond to a process.
For example, a MOP might be used for a cache that outlives any particular process.
However, the process manager will create a MOP for every process, and maintains the authority to destroy said MOP to clean up after the process.

MOPs are hierarchical.
To create one, another is needed to act as a parent and temporarily donate some of its resource limit.
If a parent is destroyed, all children are also destroyed.

A MOP capability conveys the authority to insist that parts of the virtual address space stay mapped (a claim), not necessarily to request capabilities to memory itself.
Sharing a MOP capability does _not_ give access to any memory that the MOP might have a claim to, only to make or relinquish claims.
The memory manager also does not actually have access to the memory it tracks.
The memory manager is given a reservation for all virtual space, and will give a fraction of it to the _first_ mop to make a claim for a virtual range.
Any MOP can make a subsequent claim, but they must use another channel get any capabilities to memory.

When the last MOP relinquishes its last claim for a range, the memory manager will add the range to a queue to be revoked.

### Tracking

The memory manager tracks claims on the virtual address space using a three-tiered trie.
It has basically the same structure as a three-level hierarchical page table,
but each node also has a length field (which can be any length).
This means that nodes can still be indexed fast if their start address is known, but
a single node can cover an arbitrarily long length.

Each MOP starts a doubly-linked list of ranges of memory they have a claim to.
The links of this chain are stored intrusively in the trie.
There is space for at most 4 claimants per range.

### Interface

See __mman.h__ for the interface to the memory manager.
