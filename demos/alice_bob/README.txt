Alice & Bob
===========


Introduction
------------

This walkthrough demonstrates a few of the primitives available to the programmer on CheriOS. It will teach you about the use of foundations, and how to use them to communicate over insecure channels. It will provide a short introduction to CheriOS's socket layer.


Notes
-----

Sometimes functions are defined in headers in a weird way. Rather than:

return_t foo(args);

They will appear in a macro list as

ITEM(foo, return_t, (args), ...)

This is a stopgap to allow dynamic linking of certain functions. You will see this format for functions that are part of the nanokernel, microkernel, and socket library.


Setup
-----

In this demo there are three programs: "alice", "eve", and "bob". You will have to write 'alice', the other two are written for you. If ever you get stuck check out 'alice_reference' for example solutions. You should not edit eve or bob, but you may find it educational to read them if you are wondering how the other end of the protocol is implemented.

In each example you will be required to send messages to bob, always via eve. Eve will attempt to subvert your attempts, and you will be required to use one of a number of techniques that CheriOS makes available.

Send a message
--------------

Just try to send a message to bob via eve insecurely (make sure not to send a message to bob directly!). You will need to use the namespace service to get an 'act_kt' for eve. See 'namespace.h'.

Once you have a reference to eve, you can send messages via the microkernel. See message_send in 'syscalls.h'. There also exists a message_send_c. It is the same as message_send, but message_send_c returns a capability rather than a general purpose register. You should use message_send_c.

message_send allows you to send a 4 capabilities and 5 general purpose registers as a fixed size message. See 'queue.h'. The arguments to the message_send function translate directly to the values passed in msg_t. You also provide a selector, either SEND, SEND_SWITCH, or SYNC_CALL. SEND and SEND_SWITCH are asynchronous and will give no return value. SYNC_CALL will block until the receiver calls message_return to provide a return value.

Although no special meaning is required by the microkernel, it is conventional to use v0 as a port number and another values as arguments. C1 is also special, it is not an argument to message send, rather it is a return capability constructed by the microkernel if SYNC_CALL is used.

Bob is expecting you to send him the secret message 'VERY_SECRET_DATA' in 'alice_bob.h'. Put the secret message in c3, and use the port number (v0) 'BOB_PORT_INSECURE'.

His response will also be a char*, and will be returned using the synchronous return. Use the SYNC_CALL selector to make sure you receive it.


Stop Eavesdropping
------------------

You should notice that eve is reading the message you sent! We need some way to specify that our message should only be read by bob.

In CheriOS there are two types, found_id_t*, and auth_t, which act like public and private keys respectively. A found_id_t* points to metadata about the key which allows you to assign to it an identity.

A static found_id_t 'bob_elf_id' is defined in 'bob_id.h'. This ID will contain the correct metadata. You should not use it as public key, as has no corresponding private key. Instead you should look up a public key for bob, again see 'namespace.h'. Then use a function in 'foundations.h' to check its metadata matches 'bob_elf_id'.

Once you have a found_id_t* for bob, you can lock capabilities for him. Use the nanokernel function 'rescap_take_locked' defined in 'nano_if_list.h'. This functon will require a reservation of least 'RES_CERT_META_SIZE' size. Use 'cap_malloc' to get a reservation of an appropriate size. Reservations are handles for allocations that have not yet been taken, and the nanokernel will need one to create a locked capability. You should leave the out argument null, and pass the capability argument you wish to lock as the data argument. The permissions argument is what permissions the unlocker will have on the capabilities it receives. Permissions are defined in 'cheri.h'.


Lock the same message you sent before, and send it to bob on the same port. You should note eve saying she cannot see the contents.

Stop falsifying returns
-----------------------

You may notice that the message you get from bob seems suspicious... Does it really come from bob?

Bob is willing to sign the capabilities he returns. Use the port 'BOB_PORT_SIGNED' and he will return a 'cert_t' rather than a 'char*'. Use 'rescap_check_cert' to get the found_id_t of the signer and make sure it is bob. 'rescap_check_cert' will also return a code/data pair. The data value contains bobs reply.

Send a writable capability
--------------------------

The ability to lock and sign artibtrary capabilities is very powerful. We could just use lock and sign for every message, but writable capabilities give us another option. We could lock a reference to a writable data structure, and then use memory to pass values without further foundation operations.

'alice_bob.h' defines a 'by_ref_type' type. This time, lock a 'by_ref_type*' rather than your char*, and set the message field of it to your plaintext message. Use 'BOB_PORT_BY_REFERENCE' and bob will return his response via the 'response' field.

You can allocate the memory for the 'by_ref_type' however you like. One trick is to allocate it as one block with the reservation required to lock it. If you do this, provide an 'out' argument to 'rescap_take_locked'. It will ignore the code/data arguments and instead lock the remaining space in the reservation. The unlocked copy is returned via the out argument.

Arrange an asynchronous response
--------------------------------

Any capability can now be shared via the shared memory we are passing. We could even share our own microkernel reference so bob can send an asynchronous response directly to us.

'alice_bob.h' defines a 'by_ref_type2' type. Once again allocate/lock one, and set the message to VERY_SECRET_DATA. This message also contain a nonce. A good nonce can be found by calling cap_malloc(1), which is guaranteed to give something unique and non-forgeable. Set the reply_to field to 'act_self_ref' which is our own activation reference.

This time use 'SEND_SWITCH' rather than 'SYNC_CALL', and a port number of 'BOB_PORT_ASYNC'.

You will now need to wait for the response. Call 'get_message' to wait for and receive a message. Bob will set the c3 argument as a response, and the c4 argument as your nonce. Once you have finished with the message you should call next_msg().

Intro to sockets
----------------

The ability to send arbitrary capabilities to other programs encourages a variety of shared memory data structures. However, if a data stream paradigm is required CheriOS provides a library for this purpose. A CheriOS socket has two ends, a 'requester_t' and 'fulfiller_t'. This pair forms an edge IO graph, and sockets can be connected in a graph. In this example we will create a requester. Eve will create her own fulfiller for our requester, and also her own requester matched by a fulfiller of bob.

Socket functions start socket_ and are declared in 'socket_common.h' and 'socket.h'.

First create a 'requester_t' using 'socket_malloc_requester_32'. The first argument should be 'SOCK_TYPE_PUSH' and the second NULL. Push sockets write data with requests. Pull sockets read data with requests.

Now make a reference to it using 'socket_make_ref_for_fulfill'. You should not ever pass away your handle, as this would allow whoever you pass it to to write to it. Otherwise you should always use your own reference. This time send a message with c3 as this second reference, and a port number of 'BOB_PORT_SOCKET'. Make sure to use SYNC_CALL.

After the function returns, call 'socket_requester_connect' to mark your requester as connected. Eve will have done the same on her end. You will then need to send some data. 'lorem.h' contains a good amount. First make sure there is space for a request using 'socket_requester_space_wait'. You should specify a space of 1, and 0 for dont_wait and delay_sleep. This will block until there is a space for one request.

Then make a indirect request using 'socket_request_ind'. Indrect requests specify a char& buffer and a length. Send a good amount of data if you want, the drb_offset should be 0.

Once this done, call socket_close_requester, wait_finish should be 1 and dont_wait should be 0.


Restricting Sockets
-------------------

Eve can spy on all the data she sends to bob! Although we might be happy with our data flowing along a graph made up of untrusted parties, we want a way of specifying that the sink be of a certain identity.

Use the 'socket_requester_restrict_auth' function to restrict that any data coming from us as a source and only be given to a sink that signs itself using a particular identity. You can restrict both data and out of band channels, restricting both is fine in this example.

You should note that eve cannot see your data, but can still interleave her own. This is very useful if eve needs to insert, for example, packet headers.


End of demo
-----------

If you want to see how fulfillers work, see alice. You can also see how bob signs his callback functions.