/*!
    \file    dlist.h
    \brief   Dlist function for GD32W51x WiFi SDK

    \version 2021-10-30, V1.0.0, firmware for GD32W51x
*/

/*
    Copyright (c) 2021, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#ifndef _DLIST_H_
#define _DLIST_H_

typedef struct list_head {
    struct list_head *next, *prev;
} dlist_t;

#define DLIST_HEAD_INIT(name) { &(name), &(name) }

#define DLIST_HEAD(name) \
    struct list_head name = DLIST_HEAD_INIT(name)

#define INIT_DLIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

#define list_entry(ptr, type, member) \
    ((type *)((uint8_t *)(ptr)-(uint32_t)(&((type *)0)->member)))

#define list_first(head) \
    (list_empty((head)) ? NULL : (head)->next)

#define list_last(head, type, member) \
    (list_empty((head)) ? NULL : (head)->prev)

#define list_first_entry(head, type, member) \
    (list_empty((head)) ? NULL : \
     list_entry((head)->next, type, member))

#define list_last_entry(head, type, member) \
    (list_empty((head)) ? NULL : \
     list_entry((head)->prev, type, member))

#define list_for_each(position, head) \
    for (position = (head)->next; position != (head); position = position->next)

#define list_for_each_safe(position, n, head) \
    for (position = (head)->next, n = position->next; position != (head); \
        position = n, n = position->next)

#define list_for_each_entry(position, head, type, member) \
    for (position = list_entry((head)->next, type, member); \
         &position->member != (head); \
         position = list_entry(position->member.next, type, member))

#define list_for_each_entry_safe(position, n, head, type, member) \
    for (position = list_entry((head)->next, type, member), \
           n = list_entry(position->member.next, type, member); \
         &position->member != (head); \
         position = n, n = list_entry(n->member.next, type, member))

/*!
    \brief      This function insert a new entry after the specified head
    \param[out] new: the new entry
    \param[in]  head: the specified head
    \retval     none
*/
void list_add(struct list_head *new, struct list_head *head);

/*!
    \brief      This function insert a new entry before the specified head
    \param[out] new: the new entry
    \param[in]  head: the specified head
    \retval     none
*/
void list_add_tail(struct list_head *new, struct list_head *head);

/*!
    \brief      This function deletes entry from list
    \param[in]  entry: the entry to be deleted
    \retval     none
*/
void list_del(struct list_head *entry);

/*!
    \brief      This function deletes entry from list and reinitialize it
    \param[in]  entry: the entry to be deleted
    \retval     none
*/
void list_del_init(struct list_head *entry);

/*!
    \brief      This function tests whether a list is empty
    \param[in]  head: the list head entry
    \retval     the result
*/
int list_empty(struct list_head *head);

/*!
    \brief      This function join two lists
    \param[in]  list the new list to add
    \param[in]  head the place to add it in the first list
    \retval     the result
*/
void list_splice(struct list_head *list, struct list_head *head);

#endif // _DLIST_H_
