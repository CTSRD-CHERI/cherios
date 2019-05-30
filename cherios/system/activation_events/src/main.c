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

#include "misc.h"
#include "act_events.h"
#include "object.h"
#include "stdlib.h"
#include "namespace.h"
#include "stdio.h"

/* Notes: Don't add a new event here, add to the header file and the code will be auto generated */

//TODO: We should sniff the terminate events to automatically unsubscribe terminated activations
//TODO: Not allow subsribe to terminated activation
//TODO: Not allow subsrcibe by a terminated activation
//TODO: Not allow users to trigger a notify themselves ;)

typedef struct notification_item {
    act_kt notify;
    act_kt carg;
    register_t port;
    register_t arg;
    struct notification_item* next;
} notification_item;

typedef struct notification_list {
    act_kt target;
    struct notification_item* head;
    struct notification_list* next;
} notification_list;

__unused static void dump_subs(notification_list* list) {
    while(list != NULL) {
        notification_item* item = list->head;
        CHERI_PRINT_CAP(list->target);
        while(item != NULL) {
            printf("    |--- port %lx. notify ", item->port);
            CHERI_PRINT_CAP(item->notify);
            item = item->next;
        }

        list = list->next;
    }
}

static struct notification_list** find_target(capability target, notification_list** list, int create) {

    while(*list != NULL) {
        if((*list)->target == target) {
            return list;
        }
        list = & (*list)->next;
    }

    if(create) {
        *list = (notification_list*)malloc(sizeof(notification_list));
        (*list)->target = target;
        (*list)->next = NULL;
        (*list)->head = NULL;
    }

    return list;
}

static int subscribe(act_kt target, act_kt notify, capability carg, register_t arg, register_t port, notification_list** list) {
    notification_list* target_list = *find_target(target, list, 1);

    notification_item** item;
    for(item = & target_list->head; *item != NULL; item = & (*item)->next) {
        if((*item)->notify == notify && (*item)->port == port) return SUBSCRIBE_ALREADY_SUBSCRIBED;
    }

    *item = (notification_item*)malloc(sizeof(notification_item));

    (*item)->notify = notify;
    (*item)->carg = carg;
    (*item)->arg = arg;
    (*item)->port = port;
    (*item)->next = NULL;

    return SUBSCRIBE_OK;
}

static int unsubscribe(act_kt target, act_kt notify, register_t port, notification_list** list) {
    notification_list* target_list = *find_target(target, list, 0);

    if(target_list == NULL) return SUBSCRIBE_NOT_SUBSCRIBED;


    notification_item** item;
    for(item = & target_list->head; *item != NULL; item = & (*item)->next) {
        notification_item* to_free = *item;
        if((to_free)->notify == notify && (to_free)->port == port) {
            *item = to_free->next;
            free(to_free);
            return SUBSCRIBE_OK;
        }
    }

    return SUBSCRIBE_NOT_SUBSCRIBED;
}

static int unsubscribe_all(act_kt target, act_kt notify, notification_list** list) {
    notification_list* target_list = *find_target(target, list, 0);

    if(target_list == NULL) return SUBSCRIBE_OK;

    notification_item** item;
    for(item = & target_list->head; *item != NULL; item = & (*item)->next) {
        while((*item)->notify == notify) {
            notification_item* to_free = *item;
            *item = to_free->next;
            free(to_free);
            if(*item == NULL) break;
        }
    }

    return SUBSCRIBE_OK;
}

static void notify_all(act_kt target, notification_list** list, int free_after) {

    notification_list** target_list = find_target(target, list, 0);

    if(*target_list == NULL) return;

    notification_item* item, *tmp;
    for(item = (*target_list)->head; item != NULL; item = tmp) {
        tmp = item->next;
        message_send(item->arg,0,0,0,item->carg,target,NULL,NULL,item->notify, SEND, item->port);
        if(free_after) free(item);
    }

    if(free_after) {
        notification_list* list_to_free = *target_list;
        *target_list = list_to_free->next;
        free(list_to_free);
    }
}


#define EVENT_IF_list(name, ...) name ## _list
#define EVENT_IF_def_list(name, ...) notification_list* EVENT_IF_list(name, __VA_ARGS__) = NULL;

#define EVENT_IF_BOILERPLATE_subsrcibe(name, ...)\
int __subscribe_ ## name (act_kt target, act_kt notify, capability carg, register_t arg, register_t port) {   \
return subscribe(target, notify, carg, arg, port, &EVENT_IF_list(name,...));                                \
}                                                                                                           \

#define EVENT_IF_BOILERPLATE_revoke(name, ...)\
int __unsubscribe_ ## name (act_kt target, act_kt notify, register_t port) {   \
return unsubscribe(target, notify, port, &EVENT_IF_list(name,...));                                \
}

#define EVENT_IF_BOILERPLATE_revoke_all(name, ...)\
int __unsubscribe_all_ ## name (act_kt target, act_kt notify) {   \
return unsubscribe_all(target, notify, &EVENT_IF_list(name,...));                                \
}

#define EVENT_IF_NOTIFY(name, once, ...)                    \
void __notify_ ## name (act_kt target) {               \
notify_all(target,  &EVENT_IF_list(name, once...), once);   \
}                                                           \


#define EVENT_IF_BOILERPLATE(...)                   \
    EVENT_IF_def_list(__VA_ARGS__)                  \
    EVENT_IF_NOTIFY(__VA_ARGS__)                    \
    EVENT_IF_BOILERPLATE_subsrcibe(__VA_ARGS__)     \
    EVENT_IF_BOILERPLATE_revoke(__VA_ARGS__)        \
    EVENT_IF_BOILERPLATE_revoke_all(__VA_ARGS__)


EVENT_LIST(EVENT_IF_BOILERPLATE,)

#define NAMES(name, ...) __notify_ ## name, __subscribe_ ## name, __unsubscribe_ ## name,  __unsubscribe_all_ ## name,

void (*msg_methods[]) = {EVENT_LIST(NAMES,...)};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(__unused register_t arg, __unused capability carg) {
    syscall_register_act_event_registrar(act_self_ref);
    namespace_register(namespace_num_event_service, act_self_ref);
    msg_enable = 1;
    return 0;
}
