/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "sys/types.h"
#include "klib.h"
#include "activations.h"
#include "syscalls.h"
#include "stddef.h"
#include "queue.h"
#include "mutex.h"
#include "misc.h"
#include "atomic.h"

DEFINE_ENUM_CASE(ccall_selector_t, CCALL_SELECTOR_LIST)

/*
 * Routines to handle the message queue
 */

static msg_nb_t msg_queue_fill(act_t* act) {
	return ((msg_nb_t)TRANS_HD(act->msg_tsx) - act->msg_queue->header.start);
}

int msg_push(capability c3, capability c4, capability c5, capability c6,
			 register_t a0, register_t a1, register_t a2, register_t a3,
			 register_t v0,
			 act_t * dest, act_t * src, capability sync_token) {

#if (K_DEBUG)
	src->sent_n++;
	ATOMIC_ADD_RV(&dest->recv_n, 64, 16i, 1);
#endif

	queue_t * queue = dest->msg_queue;
	msg_nb_t  qmask  = dest->queue_mask;

	uint32_t backoff_threshold = 0x1000;
	uint32_t backoff_ctr = 0;

	volatile uint64_t * tsx_ptr = &dest->msg_tsx;
	uint64_t msg_tsx;
	register_t success;

	uint64_t last_tsx = *tsx_ptr;

	while(1) {
		restart:
		LOAD_LINK(tsx_ptr, 64, msg_tsx);

		uint64_t n = TRANS_N(msg_tsx);
		uint64_t f = TRANS_N(msg_tsx);

		uint64_t head = TRANS_HD(msg_tsx);

		if(head == queue->header.start + qmask) return -1;

		if(n == f || backoff_threshold == backoff_ctr) { // Try TSX if free (n == f) or they are taking too long.
			backoff_ctr = 0;
			uint64_t add_msk = msg_tsx & N_TOP_BIT;
			uint64_t our_tsx = ((add_msk ^ msg_tsx) + N_INC) ^ add_msk; // increment n
			STORE_COND(tsx_ptr, 64, our_tsx, success);
			if(success) {
				// We are now in the TSX block. If msg_tsx changes we are being pre-empted guard each store
				msg_t* slot = &queue->msg[head & qmask];
#define GUARD_STORE(ptr, val, type, tmp)					\
                do {										\
					LOAD_LINK(ptr, type, tmp);				\
					msg_tsx = *tsx_ptr;					    \
					if(msg_tsx != our_tsx) goto restart;	\
					STORE_COND(ptr, type, val, success);	\
				} while(!success)

				capability tmp_c;

				GUARD_STORE(&slot->c3, c3, c, tmp_c);
				GUARD_STORE(&slot->c4, c4, c, tmp_c);
				GUARD_STORE(&slot->c5, c5, c, tmp_c);
				GUARD_STORE(&slot->c6, c6, c, tmp_c);

				GUARD_STORE(&slot->c1, sync_token, c, tmp_c);

				register_t tmp_r;

				GUARD_STORE(&slot->a0, a0, 64, tmp_r);
				GUARD_STORE(&slot->a1, a1, 64, tmp_r);
				GUARD_STORE(&slot->a2, a2, 64, tmp_r);
				GUARD_STORE(&slot->a3, a3, 64, tmp_r);

				GUARD_STORE(&slot->v0, v0, 64, tmp_r);

				uint64_t our_tsx_n = TRANS_N(our_tsx);
				uint64_t tsx_fin = ((((head + 1) << 16) | our_tsx_n) << 16) | our_tsx_n;
				do {
					LOAD_LINK(tsx_ptr, 64, msg_tsx);
					if(msg_tsx != our_tsx) goto restart;
					STORE_COND(tsx_ptr, 64, tsx_fin, success);
				} while(!success);

				break;
			}
		}

		if(last_tsx == msg_tsx) {
			backoff_ctr++;
		} else {
			backoff_ctr = 0;
			last_tsx = msg_tsx;
		}
		HW_YIELD; // This should look like a spin, so yield is good
	}

    sched_receive_event(dest, sched_waiting);

	KERNEL_TRACE("msg push", "now %lu items in %s's queue", msg_queue_fill(act), dest->name);

	return 0;
}

int msg_queue_empty(act_t * act) {
    return act->msg_queue->header.start == TRANS_HD(act->msg_tsx);
}

void msg_queue_init(act_t * act, queue_t * queue) {
	size_t total_length_bytes = cheri_getlen(queue);

#define printf kernel_printf
    if(total_length_bytes == 0) {
        CHERI_PRINT_CAP(queue);
        CHERI_PRINT_CAP(act);
        printf("WHo: %s", act->name);
    }
	kernel_assert(total_length_bytes > sizeof(queue_t));

	msg_nb_t queue_len = (msg_nb_t)((total_length_bytes - sizeof(queue->header)) / sizeof(msg_t));

	kernel_assert(is_power_2(queue_len));
	kernel_assert(queue_len != 0);

	act->msg_queue = queue;
	act->queue_mask = queue_len-1;
	act->msg_tsx = 0;

	act->msg_queue->header.start = 0;
	// On big endian machine pointer is to MSB, which is our head.
	msg_nb_t *hd= (msg_nb_t*)&act->msg_tsx;
	hd = cheri_setbounds(hd, sizeof(msg_nb_t));
	hd = cheri_andperm(hd, CHERI_PERM_LOAD);
	act->msg_queue->header.end = hd;
	act->msg_queue->header.len = queue_len;
	//todo: zero queue?
}

sync_indirection* alloc_new_indir(act_t* ccaller) {
    kernel_assert(ccaller->sync_state.alloc_block != NULL);
    kernel_assert(ccaller->sync_state.allocs_taken != ccaller->sync_state.allocs_max);

    res_t to_take = rescap_getsub(ccaller->sync_state.alloc_block, ccaller->sync_state.allocs_taken++);

    cap_pair pair;

    rescap_take(to_take, &pair);

    sync_indirection* si = cheri_setbounds_exact(pair.data, SI_SIZE);

    si->act = ccaller;
    si->sync_add = ccaller->sync_state.sync_token;

    return si;
}

/* Creates a token for synchronous CCalls. This ensures the answer is unique. */
static capability get_and_set_sealed_sync_token(act_t* ccaller) {

	if(ccaller->sync_state.sync_condition != 0) {
		KERNEL_ERROR("Caller %s made a sync call but seemed to be waiting for a return already\n", ccaller->name);
	}
	kernel_assert(ccaller->sync_state.sync_condition == 0);

	sync_t sn = ccaller->sync_state.sync_token;
    sync_indirection* si = ccaller->sync_state.current_sync_indir;


    if(sn - si->sync_add == MAX_SEQ_NS) {
        si = alloc_new_indir(ccaller);
    }

	ccaller->sync_state.sync_condition = 1;


    sn -=si->sync_add;
    // TODO: Generate a new sequence indirection from the free chain
    kernel_assert(sn < MAX_SEQ_NS);

    // This stores the seq number in the offset. This is fine on 256, but might be limiting depending on precision
	capability sync_token = cheri_setoffset(si, sn + MIN_OFFSET);

	return cheri_seal(sync_token, sync_token_sealer);
}

static act_t* token_expected(capability token) {
    /* Must no longer expect this sequence token */

    token = cheri_unseal(token, sync_token_sealer);

    sync_t got = cheri_getoffset(token) - MIN_OFFSET;

    sync_indirection* si= cheri_setoffset(token, 0);

    act_t* ccaller = si->act;
    got += si->sync_add;

    if(ccaller->sync_state.sync_token != got) {
		printf("got %lx. wanted %lx. (%p)\n", got, ccaller->sync_state.sync_token, &(ccaller->sync_state.sync_token));
	}

    // We might get a multithreaded return attack, so we have to atomically update the sync token

    int result;

    ATOMIC_CAS(&ccaller->sync_state.sync_token, 64, got, got+1, result);

    return result ? ccaller : NULL;
}

size_t kernel_syscall_provide_sync(res_t res) {
    res = rescap_split(res, 0); // Destroy the users handle so we can make a res field without interference
    res_nfo_t nfo = rescap_nfo(res);
    if(nfo.length < SI_SIZE) return 0;

    act_t* source_activation = (act_t*) CALLER;

    res = rescap_splitsub(res, SI_BITS);

    source_activation->sync_state.alloc_block = res;
    source_activation->sync_state.allocs_taken = 0;
    source_activation->sync_state.allocs_max = nfo.length / SI_SIZE;

    return source_activation->sync_state.allocs_max;
}

/* This function 'returns' by setting the sync state ret values appropriately */
void kernel_message_send_ret(capability c3, capability c4, capability c5, capability c6,
					 register_t a0, register_t a1, register_t a2, register_t a3,
					 act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret) {

	target_activation = act_unseal_ref(target_activation);
	act_t* source_activation = (act_t*) CALLER;

	KERNEL_TRACE(__func__, "message from %s to %s", source_activation->name, target_activation->name);

	if(target_activation->status != status_alive) {
		KERNEL_ERROR("Trying to CCall revoked activation %s from %s",
					 target_activation->name, source_activation->name);
		ret->v0 = (register_t)-1;
		ret->v1 = (register_t)-1;
		ret->c3 = NULL;
		return;
	}

	// Construct a sync_token if this is a synchronous call
	capability sync_token = NULL;
	if(selector == SYNC_CALL) {
		source_activation->sync_state.sync_ret = ret;
		sync_token = get_and_set_sealed_sync_token(source_activation);
	}

	int res;
	do {
		res = msg_push(c3, c4, c5, c6, a0, a1, a2, a3, v0, target_activation, source_activation, sync_token);
		if(res != 0) {
			kernel_printf("Message qeueue full! %s sacrifices to %s (%x)\n",
					source_activation->name,
					target_activation->name,
					target_activation->sched_status);
			sched_reschedule(target_activation, 0);
		}
	} while(res != 0);

	if(selector == SYNC_CALL) {
		sched_block_until_event(source_activation, target_activation, sched_sync_block, 0, 0);

		KERNEL_TRACE(__func__, "%s has recieved return message from %s", source_activation->name, target_activation->name);
		return;
	} else if(selector == SEND_SWITCH) {
        // No point trying to send and switch to something scheduled on another core
        if(source_activation->pool_id == target_activation->pool_id)
		    sched_reschedule(target_activation, 0);
	} else {
        // Empty
	}

	ret->v0 = 0;
	ret->v1 = 0;
	ret->c3 = NULL;
	return;
}

int kernel_message_reply(capability c3, register_t v0, register_t v1, capability sync_token) {

    act_t * returned_from = (act_t*) CALLER;

    if(sync_token == NULL) {
        KERNEL_TRACE(__func__, "%s did not provide a sync token", returned_from->name);
        kernel_freeze();
    }

    // This will handle all races in multiple calls to return
    act_t * returned_to = token_expected(sync_token);

    if(!returned_to) {
        KERNEL_ERROR("wrong sequence token from creturn");
        kernel_freeze();
    }

    kernel_assert(returned_to->sync_state.sync_ret != NULL);


	KERNEL_TRACE(__func__, "%s correctly makes a sync return to %s", returned_from->name, returned_to->name);

	/* At any point we might pre-empted, so the order here is important */

	/* First set the message to be picked up when the condition is unset */
	returned_to->sync_state.sync_ret->c3 = c3;
	returned_to->sync_state.sync_ret->v0 = v0;
	returned_to->sync_state.sync_ret->v1 = v1;

	/* Make the caller runnable again */
    sched_receive_event(returned_to, sched_sync_block);

	// TODO have a selector. Do not assume the reply wants to be descheduled
	sched_reschedule(returned_to, 0);

	return 0;
}