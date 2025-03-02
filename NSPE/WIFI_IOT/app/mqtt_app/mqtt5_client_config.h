/*  $OpenBSD: queue.h,v 1.46 2020/12/30 13:33:12 millert Exp $  */
/*  $NetBSD: queue.h,v 1.11 1996/05/16 05:17:14 mycroft Exp $   */
/*  Copyright (c) 2021, GigaDevice Semiconductor Inc.           */

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)queue.h	8.5 (Berkeley) 8/20/94
 */

/* Some adjustments in this file are made according to GigaDevice chips. */

#ifndef _MQTT5_CLIENT_CONFIG_H_
#define _MQTT5_CLIENT_CONFIG_H_

#include "app_cfg.h"

#ifdef CONFIG_MQTT
#include "lwip/apps/mqtt.h"

#define QMD_SAVELINK(name, link)    void **name = (void *)&(link)
#define TRASHIT(x) do {(x) = (void *)-1;} while (0)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_FIRST(head) ((head)->stqh_first)

#define STAILQ_HEAD(name, type)      \
struct name {        \
 struct type *stqh_first;/* first element */   \
 struct type **stqh_last;/* addr of last next element */  \
}

#define STAILQ_ENTRY(type)      \
struct {        \
    struct type *stqe_next;   \
}

#define STAILQ_INSERT_TAIL(head, elm, field) do {   \
    STAILQ_NEXT((elm), field) = NULL;    \
    *(head)->stqh_last = (elm);     \
    (head)->stqh_last = &STAILQ_NEXT((elm), field);   \
} while (0)

#define STAILQ_INIT(head) do {      \
    STAILQ_FIRST((head)) = NULL;     \
    (head)->stqh_last = &STAILQ_FIRST((head));   \
} while (0)

#define STAILQ_FOREACH(var, head, field)    \
    for((var) = STAILQ_FIRST((head));    \
        (var);       \
        (var) = STAILQ_NEXT((var), field))

#define STAILQ_FOREACH_SAFE(var, head, field, tvar)   \
    for ((var) = STAILQ_FIRST((head));    \
        (var) && ((tvar) = STAILQ_NEXT((var), field), 1);  \
        (var) = (tvar))

#define STAILQ_REMOVE_AFTER(head, elm, field) do {   \
    if ((STAILQ_NEXT(elm, field) =     \
        STAILQ_NEXT(STAILQ_NEXT(elm, field), field)) == NULL) \
    (head)->stqh_last = &STAILQ_NEXT((elm), field);  \
} while (0)

#define STAILQ_REMOVE_HEAD(head, field) do {    \
    if ((STAILQ_FIRST((head)) =     \
        STAILQ_NEXT(STAILQ_FIRST((head)), field)) == NULL)  \
    (head)->stqh_last = &STAILQ_FIRST((head));  \
} while (0)

#define STAILQ_REMOVE(head, elm, type, field) do {   \
    QMD_SAVELINK(oldnext, (elm)->field.stqe_next);   \
    if (STAILQ_FIRST((head)) == (elm)) {    \
    STAILQ_REMOVE_HEAD((head), field);   \
 }        \
 else {        \
    struct type *curelm = STAILQ_FIRST((head));  \
    while (STAILQ_NEXT(curelm, field) != (elm))  \
    curelm = STAILQ_NEXT(curelm, field);  \
    STAILQ_REMOVE_AFTER(head, curelm, field);  \
 }        \
    TRASHIT(*oldnext);      \
} while (0)

int mqtt5_client_set_connect_property(mqtt_client_t *client, const mqtt5_connection_property_config_t *connect_property);
int mqtt5_client_set_user_property(mqtt5_user_property_handle_t *user_property, mqtt5_user_property_item_hash_t item[], uint8_t item_num);
void mqtt5_client_delete_user_property(mqtt5_user_property_handle_t user_property);
void mqtt5_user_info_config_mem_free(mqtt_client_t *client);
int mqtt5_param_cfg(mqtt_client_t *mqtt_client);
void mqtt5_param_delete(mqtt_client_t *mqtt_client);

#endif // CONFIG_MQTT
#endif // _MQTT5_CLIENT_CONFIG_H_
