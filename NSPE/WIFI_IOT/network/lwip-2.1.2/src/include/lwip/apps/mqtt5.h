/*!
    \file    mqtt5.h
    \brief   application configuration for GD32W51x WiFi SDK

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
#ifndef MQTT5_H
#define MQTT5_H

#include "lwip/apps/mqtt.h"
#ifdef LWIP_MQTT
#include <stdbool.h>

typedef enum mqtt5_connect_return_res {
    MQTT5_UNSPECIFIED_ERROR                      = 0x80,
    MQTT5_MALFORMED_PACKET                       = 0x81,
    MQTT5_PROTOCOL_ERROR                         = 0x82,
    MQTT5_IMPLEMENT_SPECIFIC_ERROR               = 0x83,
    MQTT5_UNSUPPORTED_PROTOCOL_VER               = 0x84,
    MQTT5_INVAILD_CLIENT_ID                      = 0x85,
    MQTT5_BAD_USERNAME_OR_PWD                    = 0x86,
    MQTT5_NOT_AUTHORIZED                         = 0x87,
    MQTT5_SERVER_UNAVAILABLE                     = 0x88,
    MQTT5_SERVER_BUSY                            = 0x89,
    MQTT5_BANNED                                 = 0x8A,
    MQTT5_SERVER_SHUTTING_DOWN                   = 0x8B,
    MQTT5_BAD_AUTH_METHOD                        = 0x8C,
    MQTT5_KEEP_ALIVE_TIMEOUT                     = 0x8D,
    MQTT5_SESSION_TAKEN_OVER                     = 0x8E,
    MQTT5_TOPIC_FILTER_INVAILD                   = 0x8F,
    MQTT5_TOPIC_NAME_INVAILD                     = 0x90,
    MQTT5_PACKET_IDENTIFIER_IN_USE               = 0x91,
    MQTT5_PACKET_IDENTIFIER_NOT_FOUND            = 0x92,
    MQTT5_RECEIVE_MAXIMUM_EXCEEDED               = 0x93,
    MQTT5_TOPIC_ALIAS_INVAILD                    = 0x94,
    MQTT5_PACKET_TOO_LARGE                       = 0x95,
    MQTT5_MESSAGE_RATE_TOO_HIGH                  = 0x96,
    MQTT5_QUOTA_EXCEEDED                         = 0x97,
    MQTT5_ADMINISTRATIVE_ACTION                  = 0x98,
    MQTT5_PAYLOAD_FORMAT_INVAILD                 = 0x99,
    MQTT5_RETAIN_NOT_SUPPORT                     = 0x9A,
    MQTT5_QOS_NOT_SUPPORT                        = 0x9B,
    MQTT5_USE_ANOTHER_SERVER                     = 0x9C,
    MQTT5_SERVER_MOVED                           = 0x9D,
    MQTT5_SHARED_SUBSCR_NOT_SUPPORTED            = 0x9E,
    MQTT5_CONNECTION_RATE_EXCEEDED               = 0x9F,
    MQTT5_MAXIMUM_CONNECT_TIME                   = 0xA0,
    MQTT5_SUBSCRIBE_IDENTIFIER_NOT_SUPPORT       = 0xA1,
    MQTT5_WILDCARD_SUBSCRIBE_NOT_SUPPORT         = 0xA2,
} mqtt5_connect_return_res_t;

enum mqtt_properties_type {
    MQTT5_PROPERTY_PAYLOAD_FORMAT_INDICATOR      = 0x01,
    MQTT5_PROPERTY_MESSAGE_EXPIRY_INTERVAL       = 0x02,
    MQTT5_PROPERTY_CONTENT_TYPE                  = 0x03,
    MQTT5_PROPERTY_RESPONSE_TOPIC                = 0x08,
    MQTT5_PROPERTY_CORRELATION_DATA              = 0x09,
    MQTT5_PROPERTY_SUBSCRIBE_IDENTIFIER          = 0x0B,
    MQTT5_PROPERTY_SESSION_EXPIRY_INTERVAL       = 0x11,
    MQTT5_PROPERTY_ASSIGNED_CLIENT_IDENTIFIER    = 0x12,
    MQTT5_PROPERTY_SERVER_KEEP_ALIVE             = 0x13,
    MQTT5_PROPERTY_AUTHENTICATION_METHOD         = 0x15,
    MQTT5_PROPERTY_AUTHENTICATION_DATA           = 0x16,
    MQTT5_PROPERTY_REQUEST_PROBLEM_INFO          = 0x17,
    MQTT5_PROPERTY_WILL_DELAY_INTERVAL           = 0x18,
    MQTT5_PROPERTY_REQUEST_RESP_INFO             = 0x19,
    MQTT5_PROPERTY_RESP_INFO                     = 0x1A,
    MQTT5_PROPERTY_SERVER_REFERENCE              = 0x1C,
    MQTT5_PROPERTY_REASON_STRING                 = 0x1F,
    MQTT5_PROPERTY_RECEIVE_MAXIMUM               = 0x21,
    MQTT5_PROPERTY_TOPIC_ALIAS_MAXIMIM           = 0x22,
    MQTT5_PROPERTY_TOPIC_ALIAS                   = 0x23,
    MQTT5_PROPERTY_MAXIMUM_QOS                   = 0x24,
    MQTT5_PROPERTY_RETAIN_AVAILABLE              = 0x25,
    MQTT5_PROPERTY_USER_PROPERTY                 = 0x26,
    MQTT5_PROPERTY_MAXIMUM_PACKET_SIZE           = 0x27,
    MQTT5_PROPERTY_WILDCARD_SUBSCR_AVAILABLE     = 0x28,
    MQTT5_PROPERTY_SUBSCR_IDENTIFIER_AVAILABLE   = 0x29,
    MQTT5_PROPERTY_SHARED_SUBSCR_AVAILABLE       = 0x2A
};

typedef struct mqtt5_user_property_list_t *mqtt5_user_property_handle_t;

typedef struct {
    uint32_t session_expiry_interval;
    uint32_t maximum_packet_size;
    uint16_t receive_maximum;
    uint16_t topic_alias_maximum;
    bool request_resp_info;
    bool request_problem_info;
    mqtt5_user_property_handle_t user_property;
    uint32_t will_delay_interval;
    uint32_t message_expiry_interval;
    bool payload_format_indicator;
    const char *content_type;
    const char *response_topic;
    const char *correlation_data;
    uint16_t correlation_data_len;
    mqtt5_user_property_handle_t will_user_property;
} mqtt5_connection_property_config_t;

typedef struct {
    bool payload_format_indicator;
    uint32_t message_expiry_interval;
    uint16_t topic_alias;
    const char *response_topic;
    const char *correlation_data;
    uint16_t correlation_data_len;
    const char *content_type;
    mqtt5_user_property_handle_t user_property;
} mqtt5_publish_property_config_t;

typedef struct {
    uint16_t subscribe_id;
    bool no_local_flag;
    bool retain_as_published_flag;
    uint8_t retain_handle;
    bool is_share_subscribe;
    const char *share_name;
    mqtt5_user_property_handle_t user_property;
} mqtt5_subscribe_property_config_t;

typedef struct {
    bool is_share_subscribe;
    const char *share_name;
    mqtt5_user_property_handle_t user_property;
} mqtt5_unsubscribe_property_config_t;

typedef struct {
    uint32_t session_expiry_interval;
    uint8_t disconnect_reason;
    mqtt5_user_property_handle_t user_property;
} mqtt5_disconnect_property_config_t;

typedef struct {
    bool payload_format_indicator;
    char *response_topic;
    int response_topic_len;
    char *correlation_data;
    uint16_t correlation_data_len;
    char *content_type;
    int content_type_len;
    mqtt5_user_property_handle_t user_property;
} mqtt5_event_property_t;

typedef struct {
    const char *key;
    const char *value;
} mqtt5_user_property_item_hash_t;

typedef struct mqtt5_topic_alias {
    char *topic;
    uint16_t topic_len;
    uint16_t topic_alias;
    struct {
        struct mqtt5_topic_alias *stqe_next;
    } next;
} mqtt5_topic_alias_t;

struct mqtt5_topic_alias_list_t {
    struct mqtt5_topic_alias *stqh_first;
    struct mqtt5_topic_alias **stqh_last;
};

typedef struct mqtt5_topic_alias_list_t *mqtt5_topic_alias_handle_t;
typedef struct mqtt5_topic_alias *mqtt5_topic_alias_item_t;

typedef struct mqtt5_user_property {
    char *key;
    char *value;
    struct {
        struct mqtt5_user_property *stqe_next;
    } next;
} mqtt5_user_property_t;
struct mqtt5_user_property_list_t {
    struct mqtt5_user_property *stqh_first;
    struct mqtt5_user_property **stqh_last;
};
typedef struct mqtt5_user_property *mqtt5_user_property_item_t;

typedef struct {
    uint32_t maximum_packet_size;
    uint16_t receive_maximum;
    uint16_t topic_alias_maximum;
    uint8_t max_qos;
    bool retain_available;
    bool wildcard_subscribe_available;
    bool subscribe_identifiers_available;
    bool shared_subscribe_available;
    char *response_info;
} mqtt5_connection_server_resp_property_t;

typedef struct {
    bool payload_format_indicator;
    uint32_t message_expiry_interval;
    uint16_t topic_alias;
    char *response_topic;
    int response_topic_len;
    char *correlation_data;
    uint16_t correlation_data_len;
    char *content_type;
    int content_type_len;
    uint16_t subscribe_id;
} mqtt5_publish_resp_property_t;

typedef struct {
    uint32_t session_expiry_interval;
    uint32_t maximum_packet_size;
    uint16_t receive_maximum;
    uint16_t topic_alias_maximum;
    bool request_resp_info;
    bool request_problem_info;
    struct mqtt5_user_property_list_t *user_property;
} mqtt5_connection_property_storage_t;

typedef struct {
    uint32_t will_delay_interval;
    uint32_t message_expiry_interval;
    bool payload_format_indicator;
    char *content_type;
    char *response_topic;
    char *correlation_data;
    uint16_t correlation_data_len;
    struct mqtt5_user_property_list_t *user_property;
} mqtt5_connection_will_property_storage_t;

typedef struct mqtt5_config_storage{
    mqtt5_connection_property_storage_t connect_property_info;
    mqtt5_connection_will_property_storage_t will_property_info;
    mqtt5_connection_server_resp_property_t server_resp_property_info;
    mqtt5_disconnect_property_config_t disconnect_property_info;
    const mqtt5_publish_property_config_t *publish_property_info;
    const mqtt5_subscribe_property_config_t *subscribe_property_info;
    const mqtt5_unsubscribe_property_config_t *unsubscribe_property_info;
    mqtt5_topic_alias_handle_t peer_topic_alias;
} mqtt5_config_storage_t;

typedef struct topic_t{
    const char *filter;
    int qos;
} mqtt5_topic_t;

err_t mqtt5_client_connect(mqtt_client_t *client, const ip_addr_t *ip_addr, u16_t port, mqtt_connection_cb_t cb, void *arg, const struct mqtt_connect_client_info_t *client_info,
        const mqtt5_connection_property_storage_t *property, const mqtt5_connection_will_property_storage_t *will_property, u8_t mutual_auth);
err_t mqtt5_msg_publish(mqtt_client_t *client, const char *topic, const void *payload, u16_t payload_length, u8_t qos, u8_t retain,
        mqtt_request_cb_t cb, void *arg, const mqtt5_publish_property_config_t *property, const char *resp_info);
err_t mqtt5_msg_subscribe(mqtt_client_t *client, mqtt_request_cb_t cb, void *arg, const mqtt5_topic_t *topic_list,
        size_t size, u8_t retain, const mqtt5_subscribe_property_config_t *property);
err_t mqtt5_msg_unsub(mqtt_client_t *client, const char *topic, u8_t qos, mqtt_request_cb_t cb,
        void *arg, const mqtt5_unsubscribe_property_config_t *property);
void mqtt5_disconnect(mqtt_client_t *client);
#endif  //LWIP_MQTT
#endif  //MQTT5_H
