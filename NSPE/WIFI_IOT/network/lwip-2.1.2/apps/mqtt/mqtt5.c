/**
 * @file
 * MQTT
 *
 * @defgroup mqtt
 * @ingroup apps
 * @verbinclude mqtt.txt
 */

/*
 * Copyright (c) 2016 Erik Andersson <erian747@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack
 *
 * Author: Erik Andersson <erian747@gmail.com>
 *
 *
 * @todo:
 * - Handle large outgoing payloads for PUBLISH messages
 * - Fix restriction of a single topic in each (UN)SUBSCRIBE message (protocol has support for multiple topics)
 * - Add support for legacy MQTT protocol version
 *
 * Please coordinate changes and requests with Erik Andersson
 * Erik Andersson <erian747@gmail.com>
 *
 */

#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "lwip/mem.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/apps/mqtt5.h"
#include "lwip/apps/mqtt_ssl_config.h"
#include "mqtt5_client_config.h"
#include "mqtt_client_config.h"
#include <string.h>

#ifdef LWIP_MQTT

#define MQTT5_MAX_FIXED_HEADER_SIZE 5
#define APPEND_CHECK(a, ret)  if(a == -1) {                         \
        ESP_LOGE(TAG,"%s(%d) fail",__FUNCTION__, __LINE__);      \
        return (ret);                                           \
        }
#define MQTT5_SHARED_SUB "$share/%s/%s"
#define MQTT5_CONVERT_ONE_BYTE_TO_FOUR(i, a, b, c, d) i = (a << 24); \
                                                      i |= (b << 16); \
                                                      i |= (c << 8); \
                                                      i |= d;

#define MQTT5_CONVERT_ONE_BYTE_TO_TWO(i, a, b)        i = (a << 8); \
                                                      i |= b;

#define MQTT5_CONVERT_TWO_BYTE(i, a)                  i = (a >> 8) & 0xff; \
                                                      i = a & 0xff

extern mqtt5_publish_resp_property_t *resp_property;
extern mqtt5_connection_property_config_t connect_property;

static int mqtt5_property_append(struct mqtt_ringbuf_t *P_buf, uint8_t property_type, uint8_t len_occupy, const char *data, size_t data_len)
{
    if (property_type) {
        mqtt_output_append_u8(P_buf, property_type);
    }

    if (len_occupy == 0) {
        do {
            mqtt_output_append_u8(P_buf, (data_len & 0x7f) | (data_len >= 128 ? 0x80 : 0));
            data_len >>= 7;
        } while (data_len > 0);
    } else {
        for (int i = 1; i <= len_occupy; i ++) {
            mqtt_output_append_u8(P_buf, (data_len >> (8 * (len_occupy - i))) & 0xff);
        }
    }

    if (data) {
        while(data_len > 0) {
            mqtt_output_append_u8(P_buf, *data);
            data++;
            data_len--;
        }
    }
    return 0;
}

static size_t mqtt5_variable_len_get(uint8_t *buffer, size_t offset, size_t buffer_length, uint8_t *len_bytes)
{
    *len_bytes = 0;
    size_t len = 0, i = 0;
    for (i = offset; i < buffer_length; i ++) {
        len += (buffer[i] & 0x7f) << (7 * (i - offset));
        if ((buffer[i] & 0x80) == 0) {
            i ++;
            break;
        }
    }
    *len_bytes = i - offset;
    return len;
}

u16_t mqtt5_property_head_len_calc(u16_t pro_len)
{
    u16_t pro_head_len = 0;
    do {
       pro_head_len++;
       pro_len >>= 7;
    } while (pro_len > 0);

    return pro_head_len;
}

void mqtt5_property_head_len_padding(struct mqtt_ringbuf_t *P_buf, u16_t pro_len)
{
    do {
        mqtt_output_append_u8(P_buf, (pro_len & 0x7f) | (pro_len >= 128 ? 0x80 : 0));
        pro_len >>= 7;
    } while (pro_len > 0);
    return;
}

void mqtt5_connection_property_append_remain_calc(u16_t *pro_len, u16_t *pro_head_len, u16_t *will_pro_len, u16_t *will_pro_head_len, const mqtt5_connection_property_storage_t *property,
                const mqtt5_connection_will_property_storage_t *will_property, const struct mqtt_connect_client_info_t *client_info)
{
    LWIP_ASSERT("mqtt_client_connect: connect property is NULL", property != NULL);
    u16_t property_len = 0, will_property_len = 0;

    if (property->session_expiry_interval) property_len += 5;
    if (property->maximum_packet_size)     property_len += 5;
    if (property->receive_maximum)         property_len += 3;
    if (property->topic_alias_maximum)     property_len += 3;
    if (property->request_resp_info)       property_len += 2;
    if (property->request_problem_info)    property_len += 2;

    if (property->user_property) {
        mqtt5_user_property_item_t item;
        STAILQ_FOREACH(item, property->user_property, next) {
            property_len += (5 + strlen(item->key) + strlen(item->value));
        }
    }
    *pro_len           = property_len;
    *pro_head_len      = mqtt5_property_head_len_calc(property_len);

    *will_pro_len      = 0;
    *will_pro_head_len = 0;

    if (client_info->will_topic != NULL && client_info->will_topic[0] != '\0') {
        LWIP_ASSERT("mqtt_client_connect: connect will property is NULL", will_property != NULL);

        if (will_property->will_delay_interval)       will_property_len += 5;
        if (will_property->payload_format_indicator)  will_property_len += 2;
        if (will_property->message_expiry_interval)   will_property_len += 5;
        if (will_property->content_type)              will_property_len += (3 + strlen(will_property->content_type));
        if (will_property->response_topic)            will_property_len += (3 + strlen(will_property->response_topic));
        if (will_property->correlation_data && will_property->correlation_data_len)
                                                      will_property_len += (3 + strlen(will_property->correlation_data));
        if (will_property->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, will_property->user_property, next)
                will_property_len += (5 + strlen(item->key) + strlen(item->value));
        }

        *will_pro_len      = will_property_len;
        *will_pro_head_len = mqtt5_property_head_len_calc(will_property_len);
    }
    return;
}

void mqtt5_connection_property_append_padding(struct mqtt_ringbuf_t *rb, u16_t property_len, const mqtt5_connection_property_storage_t *property)
{
    LWIP_ASSERT("mqtt_client_connect: connect property is NULL", property != NULL);
    mqtt5_property_head_len_padding(rb, property_len);

    if (property->session_expiry_interval) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_SESSION_EXPIRY_INTERVAL, 4, NULL, property->session_expiry_interval);
    }
    if (property->maximum_packet_size) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_MAXIMUM_PACKET_SIZE, 4, NULL, property->maximum_packet_size);
    }
    if (property->receive_maximum) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_RECEIVE_MAXIMUM, 2, NULL, property->receive_maximum);
    }
    if (property->topic_alias_maximum) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_TOPIC_ALIAS_MAXIMIM, 2, NULL, property->topic_alias_maximum);
    }
    if (property->request_resp_info) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_REQUEST_RESP_INFO, 1, NULL, 1);
    }
    if (property->request_problem_info) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_REQUEST_PROBLEM_INFO, 1, NULL, 1);
    }
    if (property->user_property) {
        mqtt5_user_property_item_t item;
        STAILQ_FOREACH(item, property->user_property, next) {
            mqtt5_property_append(rb, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
            mqtt5_property_append(rb, 0, 2, item->value, strlen(item->value));
        }
    }
    return;
}

void mqtt5_connection_will_property_append_padding(struct mqtt_ringbuf_t *rb, u16_t will_property_len, const mqtt5_connection_will_property_storage_t *will_property,
                                    const struct mqtt_connect_client_info_t *client_info)
{
    if ( (client_info->will_topic == NULL) || (client_info->will_topic[0] == '\0')) {
        return;
    }
    LWIP_ASSERT("mqtt_client_connect: connect will property is NULL", will_property != NULL);
    mqtt5_property_head_len_padding(rb,will_property_len);

    if (will_property->will_delay_interval) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_WILL_DELAY_INTERVAL, 4, NULL, will_property->will_delay_interval);
    }
    if (will_property->payload_format_indicator) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_PAYLOAD_FORMAT_INDICATOR, 1, NULL, 1);
    }
    if (will_property->message_expiry_interval) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_MESSAGE_EXPIRY_INTERVAL, 4, NULL, will_property->message_expiry_interval);
    }
    if (will_property->content_type) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_CONTENT_TYPE, 2, will_property->content_type, strlen(will_property->content_type));
    }
    if (will_property->response_topic) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_RESPONSE_TOPIC, 2, will_property->response_topic, strlen(will_property->response_topic));
    }
    if (will_property->correlation_data && will_property->correlation_data_len) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_CORRELATION_DATA, 2, will_property->correlation_data, will_property->correlation_data_len);
    }
    if (will_property->user_property) {
        mqtt5_user_property_item_t item;
        STAILQ_FOREACH(item, will_property->user_property, next) {
            mqtt5_property_append(rb, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
            mqtt5_property_append(rb, 0, 2, item->value, strlen(item->value));
        }
    }
    return;
}

err_t mqtt5_frame_remaining_length_and_flags_calc(u8_t *flags, u16_t *remaining_length, const struct mqtt_connect_client_info_t *client_info)
{
    size_t len;
    u16_t client_user_len = 0, client_pass_len = 0;
    u8_t will_topic_len = 0, will_msg_len = 0;
    u16_t client_id_length;

    *remaining_length = 2 + 4 + 1 + 1 + 2;
    LWIP_ASSERT("mqtt_client_connect: client_info != NULL", client_info != NULL);
    LWIP_ASSERT("mqtt_client_connect: client_info->client_id != NULL", client_info->client_id != NULL);

    /* Build connect message */
    if (client_info->will_topic != NULL && client_info->will_msg != NULL) {
        *flags |= MQTT_CONNECT_FLAG_WILL;
        *flags |= (client_info->will_qos & 3) << 3;
        if (client_info->will_retain) {
            *flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
        }
        len = strlen(client_info->will_topic);
        LWIP_ERROR("mqtt_client_connect: client_info->will_topic length overflow", len <= 0xFF, return ERR_VAL);
        LWIP_ERROR("mqtt_client_connect: client_info->will_topic length must be > 0", len > 0, return ERR_VAL);
        will_topic_len = (u8_t)len;
        len = strlen(client_info->will_msg);
        LWIP_ERROR("mqtt_client_connect: client_info->will_msg length overflow", len <= 0xFF, return ERR_VAL);
        will_msg_len = (u8_t)len;
        len = *remaining_length + 2 + will_topic_len + 2 + will_msg_len;
        LWIP_ERROR("mqtt_client_connect: remaining_length overflow", len <= 0xFFFF, return ERR_VAL);
        *remaining_length = (u16_t)len;
    }
    if (client_info->client_user != NULL) {
        *flags |= MQTT_CONNECT_FLAG_USERNAME;
        len = strlen(client_info->client_user);
        LWIP_ERROR("mqtt_client_connect: client_info->client_user length overflow", len <= 0xFFFF, return ERR_VAL);
        LWIP_ERROR("mqtt_client_connect: client_info->client_user length must be > 0", len > 0, return ERR_VAL);
        client_user_len = (u16_t)len;
        len = *remaining_length + 2 + client_user_len;
        LWIP_ERROR("mqtt_client_connect: remaining_length overflow", len <= 0xFFFF, return ERR_VAL);
        *remaining_length = (u16_t)len;
    }
    if (client_info->client_pass != NULL) {
        *flags |= MQTT_CONNECT_FLAG_PASSWORD;
        len = strlen(client_info->client_pass);
        LWIP_ERROR("mqtt_client_connect: client_info->client_pass length overflow", len <= 0xFFFF, return ERR_VAL);
        LWIP_ERROR("mqtt_client_connect: client_info->client_pass length must be > 0", len > 0, return ERR_VAL);
        client_pass_len = (u16_t)len;
        len = *remaining_length + 2 + client_pass_len;
        LWIP_ERROR("mqtt_client_connect: remaining_length overflow", len <= 0xFFFF, return ERR_VAL);
        *remaining_length = (u16_t)len;
    }

    *flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;
    /* Don't complicate things, always connect using clean session */
    *flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;

    len = strlen(client_info->client_id);
    LWIP_ERROR("mqtt_client_connect: client_info->client_id length overflow", len <= 0xFFFF, return ERR_VAL);
    client_id_length = (u16_t)len;
    len = *remaining_length + 2 + client_id_length;
    LWIP_ERROR("mqtt_client_connect: remaining_length overflow", len <= 0xFFFF, return ERR_VAL);
    *remaining_length = (u16_t)len;

    return ERR_OK;
}

static void mqtt5_close(mqtt_client_t *client, mqtt_connection_status_t reason)
{
    LWIP_ASSERT("mqtt5_close: client != NULL", client != NULL);

    /* Bring down TCP connection if not already done */
    if (client->conn != NULL) {
        err_t res;
        altcp_recv(client->conn, NULL);
        altcp_err(client->conn,  NULL);
        altcp_sent(client->conn, NULL);
        res = altcp_close(client->conn);
        if (res != ERR_OK) {
            altcp_abort(client->conn);
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_close: Close err=%s\n", lwip_strerr(res)));
        }
        client->conn = NULL;
    }

    /* Remove all pending requests */
    mqtt_clear_requests(&client->pend_req_queue);
    /* Stop cyclic timer */
    sys_untimeout(mqtt_cyclic_timer, client);

    /* Notify upper layer of disconnection if changed state */
    if (client->conn_state != TCP_DISCONNECTED) {

        client->conn_state = TCP_DISCONNECTED;
        if (client->connect_cb != NULL) {
            client->connect_cb(client, client->connect_arg, reason);
        }
    }
    client->run = false;
}

mqtt_connection_status_t mqtt5_received_message_dispose(mqtt_client_t *client, u8_t fixed_hdr_idx,
                                             u16_t length, u32_t remaining_length);
static mqtt_connection_status_t mqtt5_parse_incoming(mqtt_client_t *client, struct pbuf *p)
{
    u16_t in_offset = 0;
    u32_t msg_rem_len = 0;
    u8_t fixed_hdr_idx = 0;
    u8_t b = 0;

    while (p->tot_len > in_offset) {
        /* We ALWAYS parse the header here first. Even if the header was not
           included in this segment, we re-parse it here by buffering it in
           client->rx_buffer. client->msg_idx keeps track of this. */
        if ((fixed_hdr_idx < 2) || ((b & 0x80) != 0)) {

            if (fixed_hdr_idx < client->msg_idx) {
                /* parse header from old pbuf (buffered in client->rx_buffer) */
                b = client->rx_buffer[fixed_hdr_idx];
            } else {
            /* parse header from this pbuf and save it in client->rx_buffer in case
                it comes in segmented */
                b = pbuf_get_at(p, in_offset++);
                client->rx_buffer[client->msg_idx++] = b;
           }
           fixed_hdr_idx++;

           if (fixed_hdr_idx >= 2) {
           /* fixed header contains at least 2 bytes but can contain more, depending on
               'remaining length'. All bytes but the last of this have 0x80 set to
                indicate more bytes are coming. */
                msg_rem_len |= (u32_t)(b & 0x7f) << ((fixed_hdr_idx - 2) * 7);
                if ((b & 0x80) == 0) {
                    /* fixed header is done */
                    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_parse_incoming: Remaining length after fixed header: %"U32_F"\n", msg_rem_len));
                    if (msg_rem_len == 0) {
                        /* Complete message with no extra headers of payload received */
                        mqtt5_received_message_dispose(client, fixed_hdr_idx, 0, 0);
                        client->msg_idx = 0;
                        fixed_hdr_idx = 0;
                    } else {
                        /* Bytes remaining in message (changes remaining length if this is
                        not the first segment of this message) */
                        msg_rem_len = (msg_rem_len + fixed_hdr_idx) - client->msg_idx;
                    }
                }
            }
        } else {
            /* Fixed header has been parsed, parse variable header */
            u16_t cpy_len, cpy_start, buffer_space;

            cpy_start = (client->msg_idx - fixed_hdr_idx) % (MQTT_VAR_HEADER_BUFFER_LEN - fixed_hdr_idx) + fixed_hdr_idx;

            /* Allow to copy the lesser one of available length in input data or bytes remaining in message */
            cpy_len = (u16_t)LWIP_MIN((u16_t)(p->tot_len - in_offset), msg_rem_len);

            /* Limit to available space in buffer */
            buffer_space = MQTT_VAR_HEADER_BUFFER_LEN - cpy_start;
            if (cpy_len > buffer_space) {
                cpy_len = buffer_space;
            }
            pbuf_copy_partial(p, client->rx_buffer + cpy_start, cpy_len, in_offset);

            /* Advance get and put indexes  */
            client->msg_idx += cpy_len;
            in_offset += cpy_len;
            msg_rem_len -= cpy_len;

            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt5_parse_incoming: msg_idx: %"U32_F", cpy_len: %"U16_F", remaining %"U32_F"\n", client->msg_idx, cpy_len, msg_rem_len));
            if ((msg_rem_len == 0) || (cpy_len == buffer_space)) {
                /* Whole message received or buffer is full */
                mqtt_connection_status_t res = mqtt5_received_message_dispose(client, fixed_hdr_idx, (cpy_start + cpy_len) - fixed_hdr_idx, msg_rem_len);
                if (res != MQTT_CONNECT_ACCEPTED) {
                    return res;
                }
                if (msg_rem_len == 0) {
                    /* Reset parser state */
                    client->msg_idx = 0;
                    /* msg_tot_len = 0; */
                    fixed_hdr_idx = 0;
                }
            }
        }
    }
    return MQTT_CONNECT_ACCEPTED;
}

static err_t mqtt5_tcp_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    mqtt_client_t *client = (mqtt_client_t *)arg;
    LWIP_ASSERT("mqtt_tcp_recv_cb: client != NULL", client != NULL);
    LWIP_ASSERT("mqtt_tcp_recv_cb: client->conn == pcb", client->conn == pcb);

    if (p == NULL) {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt5_tcp_recv_cb: Recv pbuf=NULL, remote has closed connection\n"));
        mqtt5_close(client, MQTT_CONNECT_DISCONNECTED);
    } else {
        mqtt_connection_status_t res;
        if (err != ERR_OK) {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt5_tcp_recv_cb: Recv err=%d\n", err));
            pbuf_free(p);
            return err;
        }

        /* Tell remote that data has been received */
        altcp_recved(pcb, p->tot_len);
        res = mqtt5_parse_incoming(client, p);
        pbuf_free(p);

        if (res != MQTT_CONNECT_ACCEPTED) {
            mqtt5_close(client, res);
        }
        /* If keep alive functionality is used */
        if (client->keep_alive != 0) {
            /* Reset server alive watchdog */
            client->server_watchdog = 0;
        }
    }
    return ERR_OK;
}

static err_t mqtt5_tcp_sent_cb(void *arg, struct altcp_pcb *tpcb, u16_t len)
{
    mqtt_client_t *client = (mqtt_client_t *)arg;

    LWIP_UNUSED_ARG(tpcb);
    LWIP_UNUSED_ARG(len);

    if (client->conn_state == MQTT_CONNECTED) {
        struct mqtt_request_t *r;

        /* Reset keep-alive send timer and server watchdog */
        client->cyclic_tick = 0;
        client->server_watchdog = 0;
        /* QoS 0 publish has no response from server, so call its callbacks here */
        while ((r = mqtt_take_request(&client->pend_req_queue, 0)) != NULL) {
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt5_tcp_sent_cb: Calling QoS 0 publish complete callback\n"));
            if (r->cb != NULL) {
                r->cb(r->arg, ERR_OK);
            }
            mqtt_delete_request(r);
        }
        /* Try send any remaining buffers from output queue */
        mqtt_output_send(&client->output, client->conn);
    }
    return ERR_OK;
}

static err_t mqtt5_tcp_poll_cb(void *arg, struct altcp_pcb *tpcb)
{
    mqtt_client_t *client = (mqtt_client_t *)arg;
    if (client->conn_state == MQTT_CONNECTED) {
        /* Try send any remaining buffers from output queue */
        mqtt_output_send(&client->output, tpcb);
    }
    return ERR_OK;
}

err_t mqtt5_tcp_connect_cb(void *arg, struct altcp_pcb *tpcb, err_t err)
{
    mqtt_client_t *client = (mqtt_client_t *)arg;

    if (err != ERR_OK) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_tcp_connect_cb: TCP connect error %d\n", err));
        return err;
    }

    /* Initiate receiver state */
    client->msg_idx = 0;

    /* Setup TCP callbacks */
    altcp_recv(tpcb, mqtt5_tcp_recv_cb);
    altcp_sent(tpcb, mqtt5_tcp_sent_cb);
    altcp_poll(tpcb, mqtt5_tcp_poll_cb, 2);

    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_tcp_connect_cb: TCP connection established to server\n"));
    /* Enter MQTT connect state */
    client->conn_state = MQTT_CONNECTING;

    /* Start cyclic timer */
    sys_timeout(MQTT_CYCLIC_TIMER_INTERVAL * 1000, mqtt_cyclic_timer, client);
    client->cyclic_tick = 0;

    /* Start transmission from output queue, connect message is the first one out*/
    mqtt_output_send(&client->output, client->conn);

    return ERR_OK;
}

err_t mqtt5_client_connect(mqtt_client_t *client, const ip_addr_t *ip_addr, u16_t port, mqtt_connection_cb_t cb, void *arg,
                                    const struct mqtt_connect_client_info_t *client_info,
                                    const mqtt5_connection_property_storage_t *property,
                                    const mqtt5_connection_will_property_storage_t *will_property, u8_t mutual)
{
    err_t err;
    u16_t remaining_length;
    u8_t flags = 0;

    u16_t property_len = 0, property_len_head_len = 0;
    u16_t will_property_len = 0, will_property_head_len = 0;

    LWIP_ASSERT_CORE_LOCKED();
    LWIP_ASSERT("mqtt_client_connect: client != NULL", client != NULL);
    LWIP_ASSERT("mqtt_client_connect: ip_addr != NULL", ip_addr != NULL);
    LWIP_ASSERT("mqtt_client_connect: client_info != NULL", client_info != NULL);
    LWIP_ASSERT("mqtt_client_connect: client_info->client_id != NULL", client_info->client_id != NULL);

    if (client->conn_state != 0) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_client_connect: Already connected\n"));
        return ERR_ISCONN;
    }

    /* Wipe clean */
    //memset(client, 0, sizeof(mqtt_client_t));
    client->connect_arg = arg;
    client->connect_cb = cb;
    client->keep_alive = client_info->keep_alive;
    mqtt_init_requests(client->req_list, LWIP_ARRAYSIZE(client->req_list));

    err = mqtt5_frame_remaining_length_and_flags_calc(&flags, &remaining_length, client_info);
    if (err != ERR_OK) {
        return err;
    }
    mqtt5_connection_property_append_remain_calc(&property_len, &property_len_head_len, &will_property_len, &will_property_head_len, property, will_property, client_info);

    remaining_length += (property_len + property_len_head_len + will_property_len + will_property_head_len);
    if (mqtt_output_check_space(&client->output, remaining_length) == 0) {
        return ERR_MEM;
    }

#if LWIP_ALTCP && LWIP_ALTCP_TLS
    mqtt_ssl_CA_cfg(client, mutual);
    if (client->tls_config) {
        client->conn = altcp_tls_new(client->tls_config, IP_GET_TYPE(ip_addr));
    } else
#endif
    {
        client->conn = altcp_tcp_new_ip_type(IP_GET_TYPE(ip_addr));
    }
    if (client->conn == NULL) {
        return ERR_MEM;
    }

    /* Set arg pointer for callbacks */
    altcp_arg(client->conn, client);
    /* Any local address, pick random local port number */
    err = altcp_bind(client->conn, IP_ADDR_ANY, 0);
    if (err != ERR_OK) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_client_connect: Error binding to local ip/port, %d\n", err));
        goto tcp_fail;
    }
    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_client_connect: Connecting to host: %s at port:%"U16_F"\n", ipaddr_ntoa(ip_addr), port));

    /* Connect to server */
    err = altcp_connect(client->conn, ip_addr, port, mqtt5_tcp_connect_cb);
    if (err != ERR_OK) {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_client_connect: Error connecting to remote ip/port, %d\n", err));
        goto tcp_fail;
    }
    /* Set error callback */
    altcp_err(client->conn, mqtt_tcp_err_cb);
    client->conn_state = TCP_CONNECTING;
    /* Append fixed header */
    mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_CONNECT, 0, 0, 0, remaining_length);
    /* Append Protocol string */
    mqtt_output_append_string(&client->output, "MQTT", 4);
    /* Append Protocol level */
    mqtt_output_append_u8(&client->output, 5);
    /* Append connect flags */
    mqtt_output_append_u8(&client->output, flags);
    /* Append keep-alive */
    mqtt_output_append_u16(&client->output, client_info->keep_alive);

    /* Append connection property */
    mqtt5_connection_property_append_padding(&client->output, property_len, property);
    /* Append client id */
    mqtt_output_append_string(&client->output, client_info->client_id, strlen(client_info->client_id));
    /* Append  connection will property id */
    mqtt5_connection_will_property_append_padding(&client->output, will_property_len, will_property, client_info);

    /* Append will message if used */
    if ((flags & MQTT_CONNECT_FLAG_WILL) != 0) {
        mqtt_output_append_string(&client->output, client_info->will_topic, strlen(client_info->will_topic));
        mqtt_output_append_string(&client->output, client_info->will_msg, strlen(client_info->will_msg));
    }
    /* Append user name if given */
    if ((flags & MQTT_CONNECT_FLAG_USERNAME) != 0) {
        mqtt_output_append_string(&client->output, client_info->client_user, strlen(client_info->client_user));
    }
    /* Append password if given */
    if ((flags & MQTT_CONNECT_FLAG_PASSWORD) != 0) {
        mqtt_output_append_string(&client->output, client_info->client_pass, strlen(client_info->client_pass));
    }
    return ERR_OK;

tcp_fail:
    altcp_abort(client->conn);
    client->conn = NULL;
    return err;
}

static int mqtt5_msg_set_user_property(mqtt5_user_property_handle_t *user_property, char *key, size_t key_len, char *value, size_t value_len)
{
    if (!*user_property) {
        *user_property = sys_calloc(1, sizeof(struct mqtt5_user_property_list_t));
        if (user_property == NULL) return ERR_MEM;

        STAILQ_INIT(*user_property);
    }

    mqtt5_user_property_item_t user_property_item = sys_calloc(1, sizeof(mqtt5_user_property_t));
    if (user_property == NULL) return ERR_MEM;
    user_property_item->key = sys_calloc(1, key_len + 1);
    if (user_property_item->key == NULL) {
        sys_mfree(user_property_item);
        return ERR_MEM;
    }
    memcpy(user_property_item->key, key, key_len);
    user_property_item->key[key_len] = '\0';

    user_property_item->value = sys_calloc(1, value_len + 1);
    if (user_property_item->value == NULL) {
        sys_mfree(user_property_item->key);
        sys_mfree(user_property_item);
        return ERR_MEM;
    }
    memcpy(user_property_item->value, value, value_len);
    user_property_item->value[value_len] = '\0';

    STAILQ_INSERT_TAIL(*user_property, user_property_item, next);
    return ERR_OK;
}

int mqtt5_msg_parse_connack_property(uint8_t *buffer, size_t buffer_len, struct mqtt_connect_client_info_t *connection_info,
                                           mqtt5_connection_property_storage_t *connection_property,
                                           mqtt5_connection_server_resp_property_t *resp_property, mqtt_connection_status_t *reason_code,
                                           uint8_t *ack_flag, mqtt5_user_property_handle_t *user_property)
{
    *reason_code = MQTT_CONNECT_ACCEPTED;
    *user_property = NULL;
    uint8_t len_bytes = 0;
    size_t offset = 1;
    size_t totlen = mqtt5_variable_len_get(buffer, offset, buffer_len, &len_bytes);
    offset += len_bytes;
    totlen += offset;

    if (totlen > buffer_len) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Total length %d is over read len %d", totlen, buffer_len));
        return ERR_ABRT;
    }

    *ack_flag = buffer[offset ++]; //acknowledge flags
    *reason_code = (mqtt_connection_status_t)buffer[offset ++]; //reason code
    size_t property_len = mqtt5_variable_len_get(buffer, offset, buffer_len, &len_bytes);
    offset += len_bytes;
    uint16_t property_offset = 0, len = 0;
    uint8_t *property = (buffer + offset);
    while (property_offset < property_len) {
        uint8_t property_id = property[property_offset ++];
        switch (property_id) {
        case MQTT5_PROPERTY_SESSION_EXPIRY_INTERVAL:
            MQTT5_CONVERT_ONE_BYTE_TO_FOUR(connection_property->session_expiry_interval, property[property_offset ++], property[property_offset ++], property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SESSION_EXPIRY_INTERVAL %d", connection_property->session_expiry_interval));
            continue;
        case MQTT5_PROPERTY_RECEIVE_MAXIMUM:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->receive_maximum, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_RECEIVE_MAXIMUM %d", resp_property->receive_maximum));
            continue;
        case MQTT5_PROPERTY_MAXIMUM_QOS:
            resp_property->max_qos = property[property_offset ++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_MAXIMUM_QOS %d", resp_property->max_qos));
            continue;
        case MQTT5_PROPERTY_RETAIN_AVAILABLE:
            resp_property->retain_available = property[property_offset ++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_RETAIN_AVAILABLE %d", resp_property->retain_available));
            continue;
        case MQTT5_PROPERTY_MAXIMUM_PACKET_SIZE:
            MQTT5_CONVERT_ONE_BYTE_TO_FOUR(resp_property->maximum_packet_size, property[property_offset ++], property[property_offset ++], property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_MAXIMUM_PACKET_SIZE %d", resp_property->maximum_packet_size));
            continue;
        case MQTT5_PROPERTY_ASSIGNED_CLIENT_IDENTIFIER:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            if (connection_info->client_id) {
                sys_mfree(connection_info->client_id);
            }
            connection_info->client_id = sys_calloc(1, len + 1);
            if (!connection_info->client_id) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Failed to calloc %d data", len));
                return ERR_ABRT;
            }
            memcpy(connection_info->client_id, &property[property_offset], len);
            connection_info->client_id[len] = '\0';
            property_offset += len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_ASSIGNED_CLIENT_IDENTIFIER %s\r\n", connection_info->client_id));
            continue;
        case MQTT5_PROPERTY_TOPIC_ALIAS_MAXIMIM:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->topic_alias_maximum, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_TOPIC_ALIAS_MAXIMIM %d", resp_property->topic_alias_maximum));
            continue;
        case MQTT5_PROPERTY_REASON_STRING: //only print now
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_REASON_STRING %s\r\n", &property[property_offset]));
            property_offset += len;
            continue;
        case MQTT5_PROPERTY_USER_PROPERTY: {
            uint8_t *key = NULL, *value = NULL;
            size_t key_len = 0, value_len = 0;
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            key = &property[property_offset];
            key_len = len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_USER_PROPERTY key: %s\r\n", (char *)key));
            property_offset += len;
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            value = &property[property_offset];
            value_len = len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_USER_PROPERTY value: %s\r\n", (char *)value));
            property_offset += len;
            if (mqtt5_msg_set_user_property(user_property, (char *)key, key_len, (char *)value, value_len) != ERR_OK) {
                mqtt5_client_delete_user_property(*user_property);
                *user_property = NULL;
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt5_msg_set_user_property fail\r\n"));
                return ERR_VAL;
            }
            continue;
        }
        case MQTT5_PROPERTY_WILDCARD_SUBSCR_AVAILABLE:
            resp_property->wildcard_subscribe_available = property[property_offset++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_WILDCARD_SUBSCR_AVAILABLE %d", resp_property->wildcard_subscribe_available));
            continue;
        case MQTT5_PROPERTY_SUBSCR_IDENTIFIER_AVAILABLE:
            resp_property->subscribe_identifiers_available = property[property_offset++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SUBSCR_IDENTIFIER_AVAILABLE %d", resp_property->subscribe_identifiers_available));
            continue;
        case MQTT5_PROPERTY_SHARED_SUBSCR_AVAILABLE:
            resp_property->shared_subscribe_available = property[property_offset++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SHARED_SUBSCR_AVAILABLE %d", resp_property->shared_subscribe_available));
            continue;
        case MQTT5_PROPERTY_SERVER_KEEP_ALIVE:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(connection_info->keep_alive, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SERVER_KEEP_ALIVE %lld", connection_info->keep_alive));
            continue;
        case MQTT5_PROPERTY_RESP_INFO:
            if (resp_property->response_info) {
                sys_mfree(resp_property->response_info);
            }
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            resp_property->response_info = sys_calloc(1, len + 1);
            if (!resp_property->response_info) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Failed to calloc %d data", len));
                return ERR_ABRT;
            }
            memcpy(resp_property->response_info, &property[property_offset], len);
            resp_property->response_info[len] = '\0';
            property_offset += len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_RESP_INFO %s\r\n", resp_property->response_info));
            continue;
        case MQTT5_PROPERTY_SERVER_REFERENCE: //only print now
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SERVER_REFERENCE %s\r\n", &property[property_offset]));
            property_offset += len;
            continue;
        case MQTT5_PROPERTY_AUTHENTICATION_METHOD: //only print now
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_AUTHENTICATION_METHOD %s\r\n", &property[property_offset]));
            property_offset += len;
            continue;
        case MQTT5_PROPERTY_AUTHENTICATION_DATA: //only print now
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_AUTHENTICATION_DATA length %d", len));
            property_offset += len;
            continue;
        default:
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("Unknown connack property id 0x%02x", property_id));
            return ERR_ABRT;
        }
    }
    return ERR_OK;
}

void mqtt5_publish_property_append_remain_calc(u16_t *pro_len, u16_t *pro_head_len, const mqtt5_publish_property_config_t *property, const char *resp_info)
{
    u16_t property_len = 0;

    if (property == NULL) {
        *pro_len = 0;
        *pro_head_len = 1;
        return;
    }

    if (property->payload_format_indicator)     property_len += 2;
    if (property->message_expiry_interval)      property_len += 5;
    if (property->topic_alias)                  property_len += 3;
    if (property->response_topic) {
        if (resp_info && strlen(resp_info)) {
            property_len += (3 + strlen(property->response_topic) + strlen(resp_info) + 1);
        } else {
            property_len += (3 + strlen(property->response_topic));
        }
    }

    if (property->correlation_data && property->correlation_data_len)
                                                property_len += (3 + property->correlation_data_len);
    if (property->user_property) {
        mqtt5_user_property_item_t item;
        STAILQ_FOREACH(item, property->user_property, next)
                                                property_len += (5 + strlen(item->key) + strlen(item->value));
    }
    if (property->content_type)                 property_len += (3 + strlen(property->content_type));

    *pro_len = property_len;
    *pro_head_len = mqtt5_property_head_len_calc(property_len);
    return;
}

void mqtt5_publish_property_append_padding(struct mqtt_ringbuf_t *rb, u16_t property_len, const char *resp_info, const mqtt5_publish_property_config_t *property)
{
    mqtt5_property_head_len_padding(rb, property_len);
    if (property == NULL) {
        return;
    }

    if (property->payload_format_indicator) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_PAYLOAD_FORMAT_INDICATOR, 1, NULL, 1);
    }
    if (property->message_expiry_interval) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_MESSAGE_EXPIRY_INTERVAL, 4, NULL, property->message_expiry_interval);
    }
    if (property->topic_alias) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_TOPIC_ALIAS, 2, NULL, property->topic_alias);
    }
    if (property->response_topic) {
        if (resp_info && strlen(resp_info)) {
            uint16_t response_topic_size = strlen(property->response_topic) + strlen(resp_info) + 1;
            char *response_topic = calloc(1, response_topic_size);
            if (!response_topic) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Failed to calloc %d memory", response_topic_size));
            }
            snprintf(response_topic, response_topic_size, "%s/%s", property->response_topic, resp_info);
            if (mqtt5_property_append(rb, MQTT5_PROPERTY_RESPONSE_TOPIC, 2, response_topic, response_topic_size) == -1) {
                LWIP_ASSERT("response_topic fail\n\r", 0);
                sys_mfree(response_topic);
                return;
            }
            sys_mfree(response_topic);
        } else {
            mqtt5_property_append(rb, MQTT5_PROPERTY_RESPONSE_TOPIC, 2, property->response_topic, strlen(property->response_topic));
        }

        if (property->correlation_data && property->correlation_data_len) {
            mqtt5_property_append(rb, MQTT5_PROPERTY_CORRELATION_DATA, 2, property->correlation_data, property->correlation_data_len);
        }
        if (property->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, property->user_property, next) {
                mqtt5_property_append(rb, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
                mqtt5_property_append(rb, 0, 2, item->value, strlen(item->value));
            }
        }
        if (property->content_type) {
            mqtt5_property_append(rb, MQTT5_PROPERTY_CONTENT_TYPE, 2, property->content_type, strlen(property->content_type));
        }
    }
    return;
}

err_t mqtt5_msg_publish(mqtt_client_t *client, const char *topic, const void *payload, u16_t payload_length, u8_t qos, u8_t retain,
             mqtt_request_cb_t cb, void *arg, const mqtt5_publish_property_config_t *property, const char *resp_info)
{
    struct mqtt_request_t *r;
    u16_t pkt_id, topic_len, remaining_length;
    size_t topic_strlen, total_len;
    u16_t property_len = 0, property_head_len = 0;

    LWIP_ASSERT("mqtt_publish: client != NULL", client);
    LWIP_ASSERT("mqtt_publish: topic != NULL", topic);
    //LWIP_ERROR("mqtt_publish: TCP disconnected", (client->conn_state != TCP_DISCONNECTED), return ERR_CONN);
    if (client->conn_state != MQTT_CONNECTED) {
        return ERR_CONN;
    }
    topic_strlen = strlen(topic);
    LWIP_ERROR("mqtt_publish: topic length overflow", (topic_strlen <= (0xFFFF - 2)), return ERR_ARG);
    topic_len = (u16_t)topic_strlen;
    total_len = 2 + topic_len + payload_length;

    if (qos > 0) {
        total_len += 2;
        pkt_id = msg_generate_packet_id(client);
    } else {
        pkt_id = 0;
    }
    LWIP_ERROR("mqtt_publish: total length overflow", (total_len <= 0xFFFF), return ERR_ARG);
    remaining_length = (u16_t)total_len;
    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_publish: Publish with payload length %d to topic \"%s\"\n", payload_length, topic));

    r = mqtt_create_request(client->req_list, LWIP_ARRAYSIZE(client->req_list), pkt_id, cb, arg);
    if (r == NULL) {
        return ERR_MEM;
    }

    mqtt5_publish_property_append_remain_calc(&property_len, &property_head_len, property, resp_info);

    remaining_length += (property_len + property_head_len);
    if (mqtt_output_check_space(&client->output, remaining_length) == 0) {
        mqtt_delete_request(r);
        return ERR_MEM;
    }
    /* Append fixed header */
    mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_PUBLISH, 0, qos, retain, remaining_length);
    /* Append Topic */
    mqtt_output_append_string(&client->output, topic, topic_len);
    /* Append packet if for QoS 1 and 2*/
    if (qos > 0) {
        mqtt_output_append_u16(&client->output, pkt_id);
    }

    mqtt5_publish_property_append_padding(&client->output, property_len, resp_info, property);

    /* Append optional publish payload */
    if ((payload != NULL) && (payload_length > 0)) {
        mqtt_output_append_buf(&client->output, payload, payload_length);
    }
    mqtt_append_request(&client->pend_req_queue, r);
    mqtt_output_send(&client->output, client->conn);
    return ERR_OK;
}

void mqtt5_subscribe_property_append_remain_calc(u16_t *total_pro_len, u16_t *pro_head_len, u16_t *pro_len, const mqtt5_topic_t *topic_list, size_t size, const mqtt5_subscribe_property_config_t *property)
{
    int property_len = 0, property_head_len = 0;
    if (property) {
        if (property->subscribe_id) {
            property_len += 1;
            property_len += mqtt5_property_head_len_calc(property->subscribe_id);
        }
        if (property->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, property->user_property, next) {
                property_len += (5 + strlen(item->key) + strlen(item->value));
            }
        }
    }
    property_head_len = mqtt5_property_head_len_calc(property_len);
    *pro_len = property_len;

    for (int topic_number = 0; topic_number < size; topic_number++) {
        if (property && property->is_share_subscribe) {
            property_len += (2 + strlen(topic_list[topic_number].filter) + strlen(MQTT5_SHARED_SUB) - 4 + strlen(property->share_name));
        } else {
            property_len += (2 + strlen(topic_list[topic_number].filter));
        }
        property_len++;
    }

    *total_pro_len = property_len;
    *pro_head_len  = property_head_len;
    return;
}

void mqtt5_subscribe_property_append_padding(struct mqtt_ringbuf_t *rb, u16_t property_len, const mqtt5_topic_t *topic_list, int size,
                                       const mqtt5_subscribe_property_config_t *property)
{
    int flags;

    mqtt5_property_head_len_padding(rb, property_len);
    if (property == NULL) {
        goto topic_list_filter;
    }

    if (property->subscribe_id) {
        mqtt5_property_append(rb, MQTT5_PROPERTY_SUBSCRIBE_IDENTIFIER, 0, NULL, property->subscribe_id);
    }
    if (property->user_property) {
        mqtt5_user_property_item_t item;
        STAILQ_FOREACH(item, property->user_property, next) {
            mqtt5_property_append(rb, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
            mqtt5_property_append(rb, 0, 2, item->value, strlen(item->value));
        }
    }

topic_list_filter:
    for (int topic_number = 0; topic_number < size; topic_number++) {
        if (topic_list[topic_number].filter[0] == '\0') {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("topic_list[%d].filter is empty", topic_number));
            return;
        }
        if (property && property->is_share_subscribe) {
            uint16_t shared_topic_size = strlen(topic_list[topic_number].filter) + strlen(MQTT5_SHARED_SUB) - 4 + strlen(property->share_name);
            char *shared_topic = sys_calloc(1, shared_topic_size);
            if (!shared_topic) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Failed to calloc %d memory", shared_topic_size));
                return;
            }
            snprintf(shared_topic, shared_topic_size + 4, MQTT5_SHARED_SUB, property->share_name, topic_list[topic_number].filter);
            if (mqtt5_property_append(rb, 0, 2, shared_topic, strlen(shared_topic)) == -1) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("append shared topic fail"));
                sys_mfree(shared_topic);
                return;
            }
            sys_mfree(shared_topic);
        } else {
            mqtt5_property_append(rb, 0, 2, topic_list[topic_number].filter, strlen(topic_list[topic_number].filter));
        }

        flags = 0;
        if (property) {
            if (property->retain_handle > 0 && property->retain_handle < 3) {
                flags |= (property->retain_handle & 3) << 4;
            }
            if (property->no_local_flag) {
                flags |= (property->no_local_flag << 2);
            }
            if (property->retain_as_published_flag) {
                flags |= (property->retain_as_published_flag << 3);
            }
        }
        flags |= (topic_list[topic_number].qos & 3);
        mqtt_output_append_u8(rb, flags);
    }
    return;
}

err_t mqtt5_msg_subscribe(mqtt_client_t *client, mqtt_request_cb_t cb, void *arg, const mqtt5_topic_t *topic_list,
                        size_t size, u8_t retain, const mqtt5_subscribe_property_config_t *property)
{
    u16_t pkt_id;
    struct mqtt_request_t *r;
    u16_t total_property_len, property_head_len, property_len;
    size_t remaining_length = 2;

    LWIP_ASSERT_CORE_LOCKED();
    LWIP_ASSERT("mqtt_sub: client != NULL", client);
    LWIP_ASSERT("mqtt_sub: topic != NULL", topic_list);

    if (client->conn_state != MQTT_CONNECTED) {
        return ERR_CONN;
    }

    if (client->conn_state == TCP_DISCONNECTED) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_sub: Can not subscribe in disconnected state\n"));
        return ERR_CONN;
    }

    mqtt5_subscribe_property_append_remain_calc(&total_property_len, &property_head_len, &property_len, topic_list, size, property);
    remaining_length += (total_property_len + property_head_len);
    if (mqtt_output_check_space(&client->output, remaining_length) == 0) {
        return ERR_MEM;
    }

    pkt_id = msg_generate_packet_id(client);
    r = mqtt_create_request(client->req_list, LWIP_ARRAYSIZE(client->req_list), pkt_id, cb, arg);
    if (r == NULL) {
        return ERR_MEM;
    }

    mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_SUBSCRIBE , 0, 1, retain, remaining_length);
    mqtt_output_append_u16(&client->output, pkt_id);
    mqtt5_subscribe_property_append_padding(&client->output, property_len, topic_list, size, property);

    mqtt_append_request(&client->pend_req_queue, r);
    mqtt_output_send(&client->output, client->conn);
    return ERR_OK;
}

err_t mqtt5_msg_unsub(mqtt_client_t *client, const char *topic, u8_t qos, mqtt_request_cb_t cb,
                      void *arg, const mqtt5_unsubscribe_property_config_t *property)
{
    u16_t topic_len;
    u16_t remaining_length = 2;
    u16_t pkt_id;
    struct mqtt_request_t *r;
    int16_t property_len_copy = 0, property_len = 0, property_len_head = 0;

    LWIP_ASSERT_CORE_LOCKED();
    LWIP_ASSERT("mqtt_unsub: client != NULL", client);
    LWIP_ASSERT("mqtt_unsub: topic != NULL", topic);

    topic_len = (u16_t)strlen(topic);

    if (property) {
        if (property->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, property->user_property, next) {
                property_len += (5 + strlen(item->key) + strlen(item->value));
            }
        }
    }

    property_len_head = mqtt5_property_head_len_calc(property_len);
    property_len_copy = property_len;
    if (property && property->is_share_subscribe) {
        property_len += (2 + topic_len + strlen(MQTT5_SHARED_SUB) - 4 + strlen(property->share_name));
    } else {
        property_len += (2 + topic_len);
    }

    remaining_length += (property_len_head + property_len);

    LWIP_ASSERT("mqtt_unsub: qos < 3", qos < 3);
    if (client->conn_state == TCP_DISCONNECTED) {
        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_unsub: Can not (un)subscribe in disconnected state\n"));
        return ERR_CONN;
    }

    if (mqtt_output_check_space(&client->output, remaining_length) == 0) {
        return ERR_MEM;
    }

    pkt_id = msg_generate_packet_id(client);
    r = mqtt_create_request(client->req_list, LWIP_ARRAYSIZE(client->req_list), pkt_id, cb, arg);
    if (r == NULL) {
        return ERR_MEM;
    }

    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_sub_unsub: Client (un)subscribe to topic \"%s\", id: %d\n", topic, pkt_id));
    mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_UNSUBSCRIBE, 0, 1, 0, remaining_length);
    /* Packet id */
    mqtt_output_append_u16(&client->output, pkt_id);
    mqtt5_property_head_len_padding(&client->output, property_len_copy);

    if (property) {
        if (property->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, property->user_property, next) {
                mqtt5_property_append(&client->output, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
                mqtt5_property_append(&client->output, 0, 2, item->value, strlen(item->value));
            }
        }
    }

    if (property && property->is_share_subscribe) {
        uint16_t shared_topic_size = topic_len + strlen(MQTT5_SHARED_SUB) - 4 + strlen(property->share_name);
        char *shared_topic = sys_calloc(1, shared_topic_size);
        if (!shared_topic) {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Failed to calloc %d memory", shared_topic_size));
        }
        snprintf(shared_topic, shared_topic_size + 4, MQTT5_SHARED_SUB, property->share_name, topic);
        if (mqtt5_property_append(&client->output, 0, 2, shared_topic, strlen(shared_topic)) == -1) {
            LWIP_ASSERT("shared_topic fail\n\r", 0);
            sys_mfree(shared_topic);
            return -1;
        }
        sys_mfree(shared_topic);
    } else {
        mqtt5_property_append(&client->output, 0, 2, topic, strlen(topic));
    }

    mqtt_append_request(&client->pend_req_queue, r);
    mqtt_output_send(&client->output, client->conn);
    return ERR_OK;
}

int mqtt5_msg_disconnect_msg_send(mqtt_client_t *client, mqtt5_disconnect_property_config_t *disconnect_property_info)
{
    int remaining_length = 0;
    int disconnect_res = 0;

    if (disconnect_property_info) {
        if (disconnect_property_info->session_expiry_interval) remaining_length += 5 ;
        if (disconnect_property_info->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, disconnect_property_info->user_property, next) {
                remaining_length += (5 + strlen(item->key) + strlen(item->value));
            }
        }
    }
    remaining_length++;
    if (mqtt_output_check_space(&client->output, remaining_length) == 0) {
        return ERR_MEM;
    }
    mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_DISCONNECT, 0, 0, 0, remaining_length);

    if (disconnect_property_info) {
        if (disconnect_property_info->session_expiry_interval) {
            mqtt5_property_append(&client->output, MQTT5_PROPERTY_SESSION_EXPIRY_INTERVAL, 4, NULL, disconnect_property_info->session_expiry_interval);
        }
        if (disconnect_property_info->user_property) {
            mqtt5_user_property_item_t item;
            STAILQ_FOREACH(item, disconnect_property_info->user_property, next) {
                mqtt5_property_append(&client->output, MQTT5_PROPERTY_USER_PROPERTY, 2, item->key, strlen(item->key));
                mqtt5_property_append(&client->output, 0, 2, item->value, strlen(item->value));
            }
        }
        if (disconnect_property_info->disconnect_reason) {
            disconnect_res = disconnect_property_info->disconnect_reason;
        }
    }
    mqtt_output_append_u8(&client->output, disconnect_res);
    return ERR_OK;
}

int mqtt5_msg_puback(mqtt_client_t *client, u16_t pkt_id)
{
    if (mqtt_output_check_space(&client->output, 3)) {
        mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_PUBACK, 0, 0, 0, 3);
        mqtt_output_append_u16(&client->output, pkt_id);
        mqtt_output_append_u8(&client->output, 0);
        mqtt_output_send(&client->output, client->conn);
    } else {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("pub_ack_rec_rel_response: OOM creating response with pkt_id: %d\n", pkt_id));
        return ERR_MEM;
    }

    return ERR_OK;
}

int mqtt5_msg_pubrec(mqtt_client_t *client, u16_t pkt_id)
{
    if (mqtt_output_check_space(&client->output, 3)) {
        mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_PUBREC, 0, 0, 0, 3);
        mqtt_output_append_u16(&client->output, pkt_id);
        mqtt_output_append_u8(&client->output, 0);
        mqtt_output_send(&client->output, client->conn);
    } else {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("pub_ack_rec_rel_response: OOM creating response with pkt_id: %d\n", pkt_id));
        return ERR_MEM;
    }

    return ERR_OK;
}

int mqtt5_msg_pubrel(mqtt_client_t *client, u16_t pkt_id)
{
    if (mqtt_output_check_space(&client->output, 3)) {
        mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_PUBREL, 0, 1, 0, 3);
        mqtt_output_append_u16(&client->output, pkt_id);
        mqtt_output_append_u8(&client->output, 0);
        mqtt_output_send(&client->output, client->conn);
    } else {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("pub_ack_rec_rel_response: OOM creating response with pkt_id: %d\n", pkt_id));
        return ERR_MEM;
    }

    return ERR_OK;
}

int mqtt5_msg_pubcomp(mqtt_client_t *client, u16_t pkt_id)
{
    if (mqtt_output_check_space(&client->output, 3)) {
        mqtt_output_append_fixed_header(&client->output, MQTT_MSG_TYPE_PUBCOMP, 0, 0, 0, 3);
        mqtt_output_append_u16(&client->output, pkt_id);
        mqtt_output_append_u8(&client->output, 0);
        mqtt_output_send(&client->output, client->conn);
    } else {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("pub_ack_rec_rel_response: OOM creating response with pkt_id: %d\n", pkt_id));
        return ERR_MEM;
    }

    return ERR_OK;
}

char *mqtt5_get_publish_property_payload(uint8_t *buffer, size_t buffer_length, mqtt5_publish_resp_property_t *resp_property,
                                        uint16_t *property_len, size_t *payload_len, mqtt5_user_property_handle_t *user_property)
{
    *user_property = NULL;
    uint8_t len_bytes = 0;
    size_t offset = 1;
    size_t totlen = mqtt5_variable_len_get(buffer, offset, buffer_length, &len_bytes);
    offset += len_bytes;
    totlen += offset;

    size_t topic_len = buffer[offset ++] << 8;
    topic_len |= buffer[offset ++] & 0xff;
    offset += topic_len;

    if (offset >= buffer_length) {
        return NULL;
    }

    if (MQTT_CTL_PACKET_QOS(buffer[0]) > 0) {
        if (offset + 2 >= buffer_length) {
            return NULL;
        }
        offset += 2;
    }

    *property_len = mqtt5_variable_len_get(buffer, offset, buffer_length, &len_bytes);
    offset += len_bytes;

    uint16_t len = 0, property_offset = 0;
    uint8_t *property = (buffer + offset);
    while (property_offset < *property_len) {
        uint8_t property_id = property[property_offset ++];
        switch (property_id) {
        case MQTT5_PROPERTY_PAYLOAD_FORMAT_INDICATOR:
            resp_property->payload_format_indicator = property[property_offset ++];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_PAYLOAD_FORMAT_INDICATOR %d", resp_property->payload_format_indicator));
            continue;
        case MQTT5_PROPERTY_MESSAGE_EXPIRY_INTERVAL:
            MQTT5_CONVERT_ONE_BYTE_TO_FOUR(resp_property->message_expiry_interval, property[property_offset ++], property[property_offset ++], property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_MESSAGE_EXPIRY_INTERVAL %d", resp_property->message_expiry_interval));
            continue;
        case MQTT5_PROPERTY_TOPIC_ALIAS:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->topic_alias, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_TOPIC_ALIAS %d", resp_property->topic_alias));
            continue;
        case MQTT5_PROPERTY_RESPONSE_TOPIC:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->response_topic_len, property[property_offset ++], property[property_offset ++])
            resp_property->response_topic = (char *)(property + property_offset);
            property_offset += resp_property->response_topic_len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_RESPONSE_TOPIC %s\r\n", resp_property->response_topic));
            continue;
        case MQTT5_PROPERTY_CORRELATION_DATA:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->correlation_data_len, property[property_offset ++], property[property_offset ++])
            resp_property->correlation_data = (char *)(property + property_offset);
            property_offset += resp_property->correlation_data_len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_CORRELATION_DATA length %d", resp_property->correlation_data_len));
            continue;
        case MQTT5_PROPERTY_SUBSCRIBE_IDENTIFIER:
            resp_property->subscribe_id = mqtt5_variable_len_get(property, property_offset, buffer_length, &len_bytes);
            property_offset += len_bytes;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_SUBSCRIBE_IDENTIFIER %d", resp_property->subscribe_id));
            continue;
        case MQTT5_PROPERTY_CONTENT_TYPE:
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(resp_property->content_type_len, property[property_offset ++], property[property_offset ++])
            resp_property->content_type = (char *)(property + property_offset);
            property_offset += resp_property->content_type_len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_CONTENT_TYPE  %s\r\n",  resp_property->content_type));
            continue;
        case MQTT5_PROPERTY_USER_PROPERTY: {
            uint8_t *key = NULL, *value = NULL;
            size_t key_len = 0, value_len = 0;
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            key = &property[property_offset];
            key_len = len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_USER_PROPERTY key: %s\r\n", (char *)key));
            property_offset += len;
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            value = &property[property_offset];
            value_len = len;
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_USER_PROPERTY value: %s\r\n", (char *)value));
            property_offset += len;
            if (mqtt5_msg_set_user_property(user_property, (char *)key, key_len, (char *)value, value_len) != ERR_OK) {
                mqtt5_client_delete_user_property(*user_property);
                *user_property = NULL;
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt5_msg_set_user_property fail\r\n"));
                return NULL;
            }
            continue;
        }
        case MQTT5_PROPERTY_REASON_STRING: //only print now
            MQTT5_CONVERT_ONE_BYTE_TO_TWO(len, property[property_offset ++], property[property_offset ++])
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("MQTT5_PROPERTY_REASON_STRING %s\r\n", &property[property_offset]));
            property_offset += len;
            continue;
        default:
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("Unknown publish property id 0x%02x]\r\n", property_id));
            return NULL;
        }
    }

    offset += property_offset;
    if (totlen <= buffer_length) {
        *payload_len = totlen - offset;
    } else {
        *payload_len = buffer_length - offset;
    }
    return (char *)(buffer + offset);
}

mqtt_connection_status_t mqtt5_received_message_dispose(mqtt_client_t *client, u8_t fixed_hdr_idx,
                                             u16_t length, u32_t remaining_length)
{
    mqtt_connection_status_t res = MQTT_CONNECT_ACCEPTED;
    u8_t *var_hdr_payload = client->rx_buffer + fixed_hdr_idx;

    size_t var_hdr_payload_bufsize = sizeof(client->rx_buffer) - fixed_hdr_idx;
    u8_t pkt_type = MQTT_CTL_PACKET_TYPE(client->rx_buffer[0]);
    u16_t pkt_id = 0;

    LWIP_ASSERT("client->msg_idx < MQTT_VAR_HEADER_BUFFER_LEN", client->msg_idx < MQTT_VAR_HEADER_BUFFER_LEN);
    LWIP_ASSERT("fixed_hdr_idx <= client->msg_idx", fixed_hdr_idx <= client->msg_idx);
    LWIP_ERROR("buffer length mismatch", fixed_hdr_idx + length <= MQTT_VAR_HEADER_BUFFER_LEN,
             return MQTT_CONNECT_DISCONNECTED);
    struct mqtt_connect_client_info_t *base_client_param = get_client_param_data_get();

    if (pkt_type == MQTT_MSG_TYPE_CONNACK) {
        uint8_t ack_flag;
        mqtt5_msg_parse_connack_property(client->rx_buffer, MQTT_VAR_HEADER_BUFFER_LEN, base_client_param, &(client->mqtt5_config->connect_property_info),
                                     &(client->mqtt5_config->server_resp_property_info), &res, &ack_flag, NULL);
        if (client->conn_state == MQTT_CONNECTING) {
            if (length < 2) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN,( "mqtt_message_received: Received short CONNACK message\n"));
                goto out_disconnect;
            }
            /* Get result code from CONNACK */
            res = (mqtt_connection_status_t)var_hdr_payload[1];
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_message_received: Connect response code %d\n", res));
            if (res == MQTT_CONNECT_ACCEPTED) {
                /* Reset cyclic_tick when changing to connected state */
                client->cyclic_tick = 0;
                client->conn_state = MQTT_CONNECTED;
                /* Notify upper layer */
                if (client->connect_cb != 0) {
                    client->connect_cb(client, client->connect_arg, res);
                }
            }
            connect_fail_reason = (int) res;
        } else {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_message_received: Received CONNACK in connected state\n"));
        }
    } else if (pkt_type == MQTT_MSG_TYPE_PINGRESP) {
        LWIP_DEBUGF(MQTT_DEBUG_TRACE, ( "mqtt_message_received: Received PINGRESP from server\n"));

    } else if (pkt_type == MQTT_MSG_TYPE_PUBLISH) {
        u16_t payload_offset = 0;
        u16_t payload_length = length;
        u8_t qos = MQTT_CTL_PACKET_QOS(client->rx_buffer[0]);
        u8_t retain = MQTT_CTL_PACKET_RETAIN(client->rx_buffer[0]);
        uint16_t property_len = 0;
        mqtt5_get_publish_property_payload(client->rx_buffer, MQTT_VAR_HEADER_BUFFER_LEN, resp_property, &property_len,
                                  (size_t *)(&payload_length), &(connect_property.user_property));
        if (client->msg_idx <= MQTT_VAR_HEADER_BUFFER_LEN) {
            /* Should have topic and pkt id*/
            u8_t *topic;
            u16_t after_topic;
            u8_t bkp;
            u16_t topic_len;
            u16_t qos_len = (qos ? 2U : 0U);
            if (length < 2 + qos_len) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN,( "mqtt_message_received: Received short PUBLISH packet\n"));
                goto out_disconnect;
            }
            topic_len = var_hdr_payload[0];
            topic_len = (topic_len << 8) + (u16_t)(var_hdr_payload[1]);
            if ((topic_len > length - (2 + qos_len)) ||
                (topic_len > var_hdr_payload_bufsize - (2 + qos_len))) {
                    LWIP_DEBUGF(MQTT_DEBUG_WARN,( "mqtt_message_received: Received short PUBLISH packet (topic)\n"));
                goto out_disconnect;
            }

            topic = var_hdr_payload + 2;
            after_topic = 2 + topic_len;
            /* Check buffer length, add one byte even for QoS 0 so that zero termination will fit */
            if ((after_topic + (qos ? 2U : 1U)) > var_hdr_payload_bufsize) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_message_received: Receive buffer can not fit topic + pkt_id\n"));
                goto out_disconnect;
            }

            /* id for QoS 1 and 2 */
            if (qos > 0) {
                if (length < after_topic + 2U) {
                    LWIP_DEBUGF(MQTT_DEBUG_WARN,( "mqtt_message_received: Received short PUBLISH packet (after_topic)\n"));
                    goto out_disconnect;
                }
                client->inpub_pkt_id = ((u16_t)var_hdr_payload[after_topic] << 8) + (u16_t)var_hdr_payload[after_topic + 1];
                after_topic += 2;
            } else {
                client->inpub_pkt_id = 0;
            }
            /* Take backup of byte after topic */
            bkp = topic[topic_len];

            int propety_len  = 0;
            do {
                propety_len >>= 7;
                propety_len |= (var_hdr_payload[after_topic] & 0x7f);
            } while ( (var_hdr_payload[after_topic++] & 0x80) > 0);

            payload_length = length - after_topic - propety_len;
            payload_offset = after_topic + propety_len;
            topic[topic_len] = 0;

            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_incomming_publish: Received message with QoS %d at topic: %s, payload length %"U32_F"\n",
                                     qos, topic, remaining_length + payload_length));
            if (client->pub_cb != NULL) {
                client->pub_cb(client->inpub_arg, (const char *)topic, topic_len);
            }
            /* Restore byte after topic */
            topic[topic_len] = bkp;
        }
        if (payload_length > 0 || remaining_length == 0) {
            if (length < (size_t)(payload_offset + payload_length)) {
                LWIP_DEBUGF(MQTT_DEBUG_WARN,( "mqtt_message_received: Received short packet (payload)\n"));
                goto out_disconnect;
            }
            client->data_cb(client->inpub_arg, var_hdr_payload + payload_offset, payload_length, remaining_length == 0 ? MQTT_DATA_FLAG_LAST : 0, retain);
            /* Reply if QoS > 0 */
            if (remaining_length == 0 && qos > 0) {
                if (qos == 1) {
                    mqtt5_msg_puback(client, client->inpub_pkt_id);
                    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_incomming_publish: Sending publish response: %s with pkt_id: %d\n",
                                       mqtt_msg_type_to_str(MQTT_MSG_TYPE_PUBACK), client->inpub_pkt_id));
                } else {
                    LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_incomming_publish: Sending publish response: %s with pkt_id: %d\n",
                                       mqtt_msg_type_to_str(MQTT_MSG_TYPE_PUBREC), client->inpub_pkt_id));
                    mqtt5_msg_pubrec(client, client->inpub_pkt_id);
                }
            }
        }
    } else {
        /* Get packet identifier */
        pkt_id = (u16_t)var_hdr_payload[0] << 8;
        pkt_id |= (u16_t)var_hdr_payload[1];
        if (pkt_id == 0) {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_message_received: Got message with illegal packet identifier: 0\n"));
            goto out_disconnect;
        }
        if (pkt_type == MQTT_MSG_TYPE_PUBREC) {
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_message_received: PUBREC, sending PUBREL with pkt_id: %d\n", pkt_id));
            mqtt5_msg_pubrel(client, pkt_id);
        } else if (pkt_type == MQTT_MSG_TYPE_PUBREL) {
            LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_message_received: PUBREL, sending PUBCOMP response with pkt_id: %d\n", pkt_id));
            mqtt5_msg_pubcomp(client, pkt_id);
        } else if (pkt_type == MQTT_MSG_TYPE_SUBACK || pkt_type == MQTT_MSG_TYPE_UNSUBACK ||
               pkt_type == MQTT_MSG_TYPE_PUBCOMP || pkt_type == MQTT_MSG_TYPE_PUBACK) {
            struct mqtt_request_t *r = mqtt_take_request(&client->pend_req_queue, pkt_id);
            if (r != NULL) {
                LWIP_DEBUGF(MQTT_DEBUG_TRACE, ("mqtt_message_received: %s response with id %d\n", mqtt_msg_type_to_str(pkt_type), pkt_id));
                if (pkt_type == MQTT_MSG_TYPE_SUBACK) {
                    if (length < 3) {
                        LWIP_DEBUGF(MQTT_DEBUG_WARN, ("mqtt_message_received: To small SUBACK packet\n"));
                        goto out_disconnect;
                    } else {
                        mqtt_incomming_suback(r, var_hdr_payload[2]);
                    }
                } else if (r->cb != NULL) {
                    r->cb(r->arg, ERR_OK);
                }
                mqtt_delete_request(r);
            } else {
                LWIP_DEBUGF(MQTT_DEBUG_WARN, ( "mqtt_message_received: Received %s reply, with wrong pkt_id: %d\n", mqtt_msg_type_to_str(pkt_type), pkt_id));
            }
        } else {
            LWIP_DEBUGF(MQTT_DEBUG_WARN, ( "mqtt_message_received: Received unknown message type: %d\n", pkt_type));
            goto out_disconnect;
        }
    }
    return res;

out_disconnect:
  return MQTT_CONNECT_DISCONNECTED;
}

extern bool auto_reconnect;
void mqtt5_disconnect(mqtt_client_t *client)
{
    LWIP_ASSERT_CORE_LOCKED();
    LWIP_ASSERT("mqtt_disconnect: client != NULL", client);
    if (mqtt_client_is_connected(client)) {
        mqtt5_msg_disconnect_msg_send(client, &(client->mqtt5_config->disconnect_property_info));
    }
    auto_reconnect = false;
    /* If connection in not already closed */
    if (client->conn_state != TCP_DISCONNECTED) {
        /* Set conn_state before calling mqtt_close to prevent callback from being called */
        client->conn_state = TCP_DISCONNECTED;
        mqtt5_close(client, (mqtt_connection_status_t)0);
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    mqtt_ssl_CA_free(client);
#endif
}

#endif //LWIP_MQTT