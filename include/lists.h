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
#ifndef CHERIOS_LISTS_H
#define CHERIOS_LISTS_H


#define DLL(T)          \
    struct T* first;    \
    struct T* last

#define DLL_LINK(T)    \
    struct T* next;     \
    struct T* prev

#define DLL_REMOVE(List, Item)                                  \
    if((Item) == (List)->first) (List)->first = (Item)->next;   \
    else (Item)->prev->next = (Item)->next;                     \
    if((Item) == (List)->last) (List)->last = (Item)->prev;     \
    else (Item)->next->prev = (Item)->prev

#define DLL_FOREACH(T, Item, List) for(T* (Item) = (List)->first; (Item) != NULL; (Item) = (Item->next))

#define DLL_FOREACH_RESET(T, Item, List) do {Item = (List)->first; continue; } while(0)

#define DLL_ADD_START(List, Item)                       \
    (Item)->prev = NULL;                                \
    (Item)->next = (List)->first;                       \
    if((List)->first == NULL) (List)->last = (Item);    \
    else (List)->first->prev = (Item);                  \
    (List)->first = (Item)

#define DLL_ADD_END(List, Item)                         \
    (Item)->prev = (List)->last;                        \
    (Item)->next = NULL;                                \
    if((List)->last == NULL) (List)->first = (Item);    \
    else (List)->last->next = (Item);                   \
    (List)->last = (Item)


#endif //CHERIOS_LISTS_H
