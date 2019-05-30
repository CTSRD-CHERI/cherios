/*-
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

#include "cheric.h"
#include "object.h"
#include "stdlib.h"
#include "lists.h"
#include "string.h"
#include "type_manager.h"
#include "misc.h"
#include "namespace.h"

// We can use this to check which types have actually used
type_res_bitfield_t* bitfield;

// This is our own tracking of which types we have allocated to which tops
ownership_tracker_t ownership_map[USER_TYPES_LEN];
struct free_list {
    DLL(ownership_tracker_t);
} freeList;

struct freed_list {
    DLL(ownership_tracker_t);
} freedList;


// The root top (LOL)
top_internal_t top_top;

// The sealing capability for tops
sealing_cap top_sealing_cap;




static top_internal_t* unseal_top(top_t top) {
    top_internal_t* unsealed = (top_internal_t*)cheri_unseal_2(top, top_sealing_cap);
    if(unsealed == NULL) return NULL;

    enum top_state result;

    ENUM_VMEM_SAFE_DEREFERENCE(&(unsealed->state), result, destroyed);

    if(result != created) return NULL;
    return unsealed;
}

static top_t seal_top(top_internal_t* top) {
    return cheri_seal(top, top_sealing_cap);
}

__unused static int is_taken(stype type) {
    type = type - NANO_TYPES;
    char c = bitfield->bitfield[type >> 3];
    stype sub_index = 1U << (type & 0x7);
    return (c & sub_index) != 0;
}

static stype tracker_to_type(ownership_tracker_t* tracker) {

    long diff = ((long)tracker-(long)ownership_map);
    stype index = (stype)(diff/sizeof(ownership_tracker_t));
    return index+USER_TYPES_START;
}

static ownership_tracker_t* type_to_tracker(stype type) {
    return ownership_map + (type-USER_TYPES_LEN);
}

static tres_t new_tres(ownership_tracker_t* tracker, top_internal_t* top) {

    // Dumb scan to find a tres

    stype type = tracker_to_type(tracker);
    tracker->owner = top;

    DLL_REMOVE(&freeList, tracker);
    DLL_ADD_END(&top->owns, tracker);

    top->types_allocated++;

    return tres_get(type);
}

static ownership_tracker_t* find_free_tracker(void) {
    return freeList.first;
}

static top_t new_top(top_internal_t* from_top) {
    top_internal_t* top = (top_internal_t*)malloc(sizeof(top_internal_t)); // Create new top
    DLL_ADD_END(&from_top->children, top);     // set as child
    top->parent = from_top;
    return seal_top(top);
}

static void release_type(top_internal_t* top, ownership_tracker_t* tracker, int remove_from_chain) {

    tracker->owner = OWNER_FREED;

    if(remove_from_chain) {
        DLL_REMOVE(&(top->owns), tracker);
    }

    DLL_ADD_END(&freedList, tracker);

    top->types_freed++;
}

static void destroy_top(top_internal_t* top) {

    top->state = destroying;

    // First destroy each child

    DLL_FOREACH(top_internal_t, item, &top->children) {
        destroy_top(item);
    }

    // Then release each type

    DLL_FOREACH(ownership_tracker_t, item, &top->owns) {
        release_type(top, item, 0);
    }

    // Set to destroyed
    top->state = destroyed;

    // Free

    free(top);
}


static void init_tracking(void) {
    freeList.first = ownership_map;
    freeList.last = &ownership_map[USER_TYPES_LEN-1];

    ownership_tracker_t* prev = NULL;

    for(ownership_tracker_t* cur = ownership_map; cur != ownership_map + USER_TYPES_LEN; cur++) {
        cur->prev = prev;
        cur->owner = OWNER_FREE;
        cur->next = cur+1;
    }

    freeList.last->next = NULL;
}

static top_internal_t* init_top(void) {
    bzero(&top_top, sizeof(top_internal_t));
    return &top_top;
}




top_t __type_get_first_top(void) {
    static int init = 0;

    if(!init) {
        init = 1;
        top_internal_t* first = init_top();
        return seal_top(first);
    } return NULL;
}

ERROR_T(top_t) __type_new_top(top_t parent) {
    top_internal_t* itop = unseal_top(parent);
    if(itop == NULL) return MAKE_ER(top_t, TYPE_ER_INVALID_TOP);

    return MAKE_VALID(top_t, new_top(itop));
}

er_t __type_destroy_top(top_t top) {
    top_internal_t* itop = unseal_top(top);
    if(itop == NULL) return TYPE_ER_INVALID_TOP;

    destroy_top(itop);

    return TYPE_OK;
}

ERROR_T(tres_t) __type_get_new(top_t top) {
    top_internal_t* itop = unseal_top(top);
    if(itop == NULL) return MAKE_ER(tres_t, TYPE_ER_INVALID_TOP);

    ownership_tracker_t* tracker = find_free_tracker();

    if(tracker == NULL) MAKE_ER(tres_t, TYPE_ER_OUT_OF_TYPES);

    return MAKE_VALID(tres_t,new_tres(tracker, itop));
}

ERROR_T(tres_t) __type_get_new_exact(top_t top, stype type) {
    top_internal_t* itop = unseal_top(top);
    if(itop == NULL) return MAKE_ER(tres_t, TYPE_ER_INVALID_TOP);

    if(type < USER_TYPES_START || type >= USER_TYPES_END) return MAKE_ER(tres_t, TYPE_ER_OUT_OF_RANGE);

    ownership_tracker_t* tracker = type_to_tracker(type);

    if(tracker->owner != OWNER_FREE) return MAKE_ER(tres_t, TYPE_ER_TYPE_USED);

    return MAKE_VALID(tres_t,new_tres(tracker, itop));
}

er_t __type_return_type(top_t top, stype type) {
    top_internal_t* itop = unseal_top(top);
    if(itop == NULL) return TYPE_ER_INVALID_TOP;

    if(type < USER_TYPES_START || type >= USER_TYPES_END) return TYPE_ER_OUT_OF_RANGE;

    ownership_tracker_t* tracker = type_to_tracker(type);

    if(tracker->owner != itop) return TYPE_ER_DOES_NOT_OWN;

    release_type(itop, tracker, 1);

    return TYPE_OK;
}

void (*msg_methods[]) = {&__type_get_first_top, &__type_new_top, &__type_destroy_top,
                         &__type_get_new, &__type_get_new_exact, &__type_return_type};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(__unused register_t arg, __unused capability carg) {
    bitfield = (type_res_bitfield_t*)tres_get_ro_bitfield();
    top_sealing_cap = get_sealing_cap_from_nano(TOP_SEALING_TYPE);

    init_tracking();

    namespace_register(namespace_num_tman, act_self_ref);

    msg_enable = 1;

    return 0;
}


// TODO revocation