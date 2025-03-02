/**
 * @file
 * MQTT client
 */

/*
 * Copyright (c) 2016 Erik Andersson
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
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Erik Andersson
 *
 */
#ifndef LWIP_HDR_APPS_MQTT_CLIENT_H
#define LWIP_HDR_APPS_MQTT_CLIENT_H

#include "lwip/apps/mqtt_opts.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/prot/iana.h"
#include "lwip/altcp.h"

#ifdef LWIP_MQTT
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mqtt_client_s mqtt_client_t;

#if LWIP_ALTCP && LWIP_ALTCP_TLS
struct altcp_tls_config;
#endif

/**
 * MQTT_DEBUG: Default is off.
 */
#if !defined MQTT_DEBUG || defined __DOXYGEN__
#define MQTT_DEBUG                  LWIP_DBG_OFF
#endif

#define MQTT_DEBUG_TRACE        (MQTT_DEBUG | LWIP_DBG_TRACE)
#define MQTT_DEBUG_STATE        (MQTT_DEBUG | LWIP_DBG_STATE)
#define MQTT_DEBUG_WARN         (MQTT_DEBUG | LWIP_DBG_LEVEL_WARNING)
#define MQTT_DEBUG_WARN_STATE   (MQTT_DEBUG | LWIP_DBG_LEVEL_WARNING | LWIP_DBG_STATE)
#define MQTT_DEBUG_SERIOUS      (MQTT_DEBUG | LWIP_DBG_LEVEL_SERIOUS)

/** @ingroup mqtt
 * Default MQTT port (non-TLS) */
#define MQTT_PORT     LWIP_IANA_PORT_MQTT
/** @ingroup mqtt
 * Default MQTT TLS port */
#define MQTT_TLS_PORT LWIP_IANA_PORT_SECURE_MQTT

/** Helpers to extract control packet type and qos from first byte in fixed header */
#define MQTT_CTL_PACKET_TYPE(fixed_hdr_byte0) ((fixed_hdr_byte0 & 0xf0) >> 4)
#define MQTT_CTL_PACKET_QOS(fixed_hdr_byte0) ((fixed_hdr_byte0 & 0x6) >> 1)
#define MQTT_CTL_PACKET_RETAIN(fixed_hdr_byte0) (fixed_hdr_byte0 & 0x1)
extern int16_t connect_fail_reason;


typedef enum mqtt_connect_return_res {
    MQTT_CONNECTION_ACCEPTED = 0,    /*!< Connection accepted  */
    MQTT_CONNECTION_REFUSE_PROTOCOL, /*!< *MQTT* connection refused reason: Wrong
                                      protocol */
    MQTT_CONNECTION_REFUSE_ID_REJECTED, /*!< *MQTT* connection refused reason: ID
                                         rejected */
    MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE, /*!< *MQTT* connection refused
                                                reason: Server unavailable */
    MQTT_CONNECTION_REFUSE_BAD_USERNAME,  /*!< *MQTT* connection refused reason:
                                           Wrong user */
    MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED /*!< *MQTT* connection refused reason:
                                           Wrong username or password */
} mqtt_connect_return_res_t;

enum {
  TCP_DISCONNECTED,
  TCP_CONNECTING,
  MQTT_CONNECTING,
  MQTT_CONNECTED
};

/**
 * @ingroup mqtt
 * Client information and connection parameters */
struct mqtt_connect_client_info_t {
  /** Client identifier, must be set by caller */
  char *client_id;
  /** User name, set to NULL if not used */
  char* client_user;
  /** Password, set to NULL if not used */
  char* client_pass;
  /** keep alive time in seconds, 0 to disable keep alive functionality*/
  u16_t keep_alive;
  /** will topic, set to NULL if will is not to be used,
      will_msg, will_qos and will retain are then ignored */
  char* will_topic;
  /** will_msg, see will_topic */
  char* will_msg;
  /** will_qos, see will_topic */
  u8_t will_qos;
  /** will_retain, see will_topic */
  u8_t will_retain;
};

/**
 * @ingroup mqtt
 * Function prototype for mqtt request callback. Called when a subscribe, unsubscribe
 * or publish request has completed
 * @param arg Pointer to user data supplied when invoking request
 * @param err ERR_OK on success
 *            ERR_TIMEOUT if no response was received within timeout,
 *            ERR_ABRT if (un)subscribe was denied
 */
typedef void (*mqtt_request_cb_t)(void *arg, err_t err);
/**
 * MQTT control message types
 */
enum mqtt_message_type {
  MQTT_MSG_TYPE_CONNECT = 1,
  MQTT_MSG_TYPE_CONNACK = 2,
  MQTT_MSG_TYPE_PUBLISH = 3,
  MQTT_MSG_TYPE_PUBACK = 4,
  MQTT_MSG_TYPE_PUBREC = 5,
  MQTT_MSG_TYPE_PUBREL = 6,
  MQTT_MSG_TYPE_PUBCOMP = 7,
  MQTT_MSG_TYPE_SUBSCRIBE = 8,
  MQTT_MSG_TYPE_SUBACK = 9,
  MQTT_MSG_TYPE_UNSUBSCRIBE = 10,
  MQTT_MSG_TYPE_UNSUBACK = 11,
  MQTT_MSG_TYPE_PINGREQ = 12,
  MQTT_MSG_TYPE_PINGRESP = 13,
  MQTT_MSG_TYPE_DISCONNECT = 14
};

typedef enum
{
  /** Accepted */
  MQTT_CONNECT_ACCEPTED                 = 0,
  /** Refused protocol version */
  MQTT_CONNECT_REFUSED_PROTOCOL_VERSION = 1,
  /** Refused identifier */
  MQTT_CONNECT_REFUSED_IDENTIFIER       = 2,
  /** Refused server */
  MQTT_CONNECT_REFUSED_SERVER           = 3,
  /** Refused user credentials */
  MQTT_CONNECT_REFUSED_USERNAME_PASS    = 4,
  /** Refused not authorized */
  MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_  = 5,
  /** Disconnected */
  MQTT_CONNECT_DISCONNECTED             = 256,
  /** Timeout */
  MQTT_CONNECT_TIMEOUT                  = 257
} mqtt_connection_status_t;

enum {
  /** Flag set when last fragment of data arrives in data callback */
  MQTT_DATA_FLAG_LAST = 1
};

/**
 * MQTT connect flags, only used in CONNECT message
 */
enum mqtt_connect_flag {
  MQTT_CONNECT_FLAG_USERNAME = 1 << 7,
  MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,
  MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,
  MQTT_CONNECT_FLAG_WILL = 1 << 2,
  MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1
};

/**
 * @ingroup mqtt
 * Function prototype for MQTT incoming publish data callback function. Called when data
 * arrives to a subscribed topic @see mqtt_subscribe
 *
 * @param arg Additional argument to pass to the callback function
 * @param data User data, pointed object, data may not be referenced after callback return,
          NULL is passed when all publish data are delivered
 * @param len Length of publish data fragment
 * @param flags MQTT_DATA_FLAG_LAST set when this call contains the last part of data from publish message
 *
 */
typedef void (*mqtt_incoming_data_cb_t)(void *arg, const u8_t *data, u16_t len, u8_t flags, u8_t retain);
typedef void (*mqtt_connection_cb_t)(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
typedef void (*mqtt_incoming_publish_cb_t)(void *arg, const char *topic, u32_t tot_len);
typedef struct mqtt_ringbuf_t mqtt_ringbuf;

u16_t msg_generate_packet_id(mqtt_client_t *client);
err_t mqtt_client_connect(mqtt_client_t *client, const ip_addr_t *ipaddr, u16_t port, mqtt_connection_cb_t cb, void *arg,
                   const struct mqtt_connect_client_info_t *client_info,u8_t mutual);
struct mqtt_request_t *mqtt_create_request(struct mqtt_request_t *r_objs, size_t r_objs_len, u16_t pkt_id,
                   mqtt_request_cb_t cb, void *arg);
u8_t mqtt_output_check_space(mqtt_ringbuf *rb, u16_t r_length);
void mqtt_disconnect(mqtt_client_t *client);
void mqtt_cyclic_timer(void *arg);
mqtt_client_t *mqtt_client_new(void);
void mqtt_client_free(mqtt_client_t* client);
void mqtt_clear_requests(struct mqtt_request_t **tail);
u8_t mqtt_client_is_connected(mqtt_client_t *client);
void mqtt_set_inpub_callback(mqtt_client_t *client, mqtt_incoming_publish_cb_t,
                             mqtt_incoming_data_cb_t data_cb, void *arg);
err_t mqtt_sub_unsub(mqtt_client_t *client, const char *topic, u8_t qos, u8_t retain, mqtt_request_cb_t cb, void *arg, u8_t sub);

/** @ingroup mqtt
 *Subscribe to topic */
#define mqtt_subscribe(client, topic, qos, cb, arg) mqtt_sub_unsub(client, topic, qos, cb, arg, 1)
/** @ingroup mqtt
 *  Unsubscribe to topic */
#define mqtt_unsubscribe(client, topic, cb, arg) mqtt_sub_unsub(client, topic, 0, cb, arg, 0)

err_t mqtt_msg_publish(mqtt_client_t *client, const char *topic, const void *payload, u16_t payload_length, u8_t qos, u8_t retain,
                                    mqtt_request_cb_t cb, void *arg);

void mqtt_output_append_u8(mqtt_ringbuf *rb, u8_t value);
void mqtt_output_append_u16(mqtt_ringbuf *rb, u16_t value);
void mqtt_output_append_buf(mqtt_ringbuf *rb, const void *data, u16_t length);
void mqtt_output_append_string(mqtt_ringbuf *rb, const char *str, u16_t length);
void mqtt_init_requests(struct mqtt_request_t *r_objs, size_t r_objs_len);
err_t mqtt_tcp_connect_cb(void *arg, struct altcp_pcb *tpcb, err_t err);
void mqtt_output_append_fixed_header(mqtt_ringbuf *rb, u8_t msg_type, u8_t fdup,
                                u8_t fqos, u8_t fretain, u16_t r_length);
void mqtt_tcp_err_cb(void *arg, err_t err);
void mqtt_delete_request(struct mqtt_request_t *r);
void mqtt_append_request(struct mqtt_request_t **tail, struct mqtt_request_t *r);
void mqtt_output_send(mqtt_ringbuf *rb, struct altcp_pcb *tpcb);
err_t pub_ack_rec_rel_response(mqtt_client_t *client, u8_t msg, u16_t pkt_id, u8_t qos);
struct mqtt_request_t *mqtt_take_request(struct mqtt_request_t **tail, u16_t pkt_id);
void mqtt_incomming_suback(struct mqtt_request_t *r, u8_t result);
const char * mqtt_msg_type_to_str(u8_t msg_type);
#ifdef __cplusplus
}
#endif
#endif /*LWIP_MQTT*/

#endif /* LWIP_HDR_APPS_MQTT_CLIENT_H */
