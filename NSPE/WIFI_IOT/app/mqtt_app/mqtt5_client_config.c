/*!
    \file    mqtt5_client_config.c
    \brief   MQTT version 5 client for GD32W51x WiFi SDK

    \version 2023-6-12, V1.0.3, firmware for GD32W51x
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
#include "app_cfg.h"

#ifdef CONFIG_MQTT
#include <string.h>
#include "mqtt_cmd.h"
#include "cmd_shell.h"
#include "lwip/apps/mqtt5.h"
#include "lwip/apps/mqtt_priv.h"
#include "mqtt5_client_config.h"
#include "wrapper_os.h"

#define app_print DEBUGPRINT

#define MQTT5_MEM_CHECK(a, action) if(!(a)) {                     \
    app_print("Memory exhausted!\r\n");                           \
    action;                                                       \
}

mqtt5_user_property_item_hash_t user_property_arr[3] = {
    {"board", "digadevicw"},
    {"u", "user"},
    {"p", "password"}
};

mqtt5_connection_property_config_t connect_property = {
    .session_expiry_interval = 10,
    .maximum_packet_size = 1024,
    .receive_maximum = 65535,
    .topic_alias_maximum = 2,
    .request_resp_info = true,
    .request_problem_info = true,
};

mqtt5_publish_property_config_t publish_property = {
    .payload_format_indicator = 1,
    .message_expiry_interval = 1000,
    .topic_alias = 0,
    .response_topic = "/topic/test/response",
    .correlation_data = "123456",
    .correlation_data_len = 6,
    .content_type = "json",
    .user_property = NULL,
};

mqtt5_subscribe_property_config_t subscrible_property = {
    .subscribe_id = 25555,
    .no_local_flag = false,
    .retain_as_published_flag = false,
    .retain_handle = 0,
    .is_share_subscribe = false,
    .share_name = NULL,
    .user_property = NULL,
};

mqtt5_unsubscribe_property_config_t unsubscrible_property = {
    .is_share_subscribe = true,
    .share_name = "group1",
    .user_property = NULL,
};

mqtt5_disconnect_property_config_t disconnect_property = {
    .session_expiry_interval = 60,
    .disconnect_reason = 0,
    .user_property = NULL,
};

char *string_dump(const char *s)
{
    uint32_t len = strlen(s) + 1;

    void *new = (void *)sys_malloc(len);
    if (new == NULL) {
        return NULL;
    }
    memcpy(new, s, len);

    return (char *)new;
}

int mqtt5_client_set_user_property(mqtt5_user_property_handle_t *user_property, mqtt5_user_property_item_hash_t item[], uint8_t item_num)
{
    if (!item_num || !item) {
        app_print("Input value is NULL\r\n");
        return ERR_ARG;
    }

    if (!*user_property) {
        *user_property = sys_calloc(1, sizeof(struct mqtt5_user_property_list_t));
        MQTT5_MEM_CHECK(*user_property, return ERR_MEM);
        STAILQ_INIT(*user_property);
    }

    for (int i = 0; i < item_num; i++) {
        if (item[i].key && item[i].value) {
            mqtt5_user_property_item_t user_property_item = sys_calloc(1, sizeof(mqtt5_user_property_t));
            MQTT5_MEM_CHECK(user_property_item, goto err);
            size_t key_len = strlen(item[i].key);
            size_t value_len = strlen(item[i].value);

            user_property_item->key = sys_calloc(1, key_len + 1);
            MQTT5_MEM_CHECK(user_property_item->key, {
                sys_mfree(user_property_item);
                goto err;
            });
            memcpy(user_property_item->key, item[i].key, key_len);
            user_property_item->key[key_len] = '\0';

            user_property_item->value = sys_calloc(1, value_len + 1);
            MQTT5_MEM_CHECK(user_property_item->value, {
                sys_mfree(user_property_item->key);
                sys_mfree(user_property_item);
                goto err;
            });
            memcpy(user_property_item->value, item[i].value, value_len);
            user_property_item->value[value_len] = '\0';

            STAILQ_INSERT_TAIL(*user_property, user_property_item, next);
        }
    }
    return ERR_OK;
err:
    mqtt5_client_delete_user_property(*user_property);
    *user_property = NULL;
    return ERR_MEM;
}

static int mqtt5_user_property_copy(mqtt5_user_property_handle_t user_property_new, const mqtt5_user_property_handle_t user_property_old)
{
    if (!user_property_new || !user_property_old) {
        app_print("Input is NULL\r\n");
        return ERR_ARG;
    }

    mqtt5_user_property_item_t old_item, new_item;
    STAILQ_FOREACH(old_item, user_property_old, next) {
        new_item = sys_calloc(1, sizeof(mqtt5_user_property_t));
        MQTT5_MEM_CHECK(new_item, return -1);
        new_item->key = string_dump(old_item->key);
        MQTT5_MEM_CHECK(new_item->key, {
            sys_mfree(new_item);
            return ERR_MEM;
        });
        new_item->value = string_dump(old_item->value);
        MQTT5_MEM_CHECK(new_item->value, {
            sys_mfree(new_item->key);
            sys_mfree(new_item);
            return ERR_MEM;
        });
        STAILQ_INSERT_TAIL(user_property_new, new_item, next);
    }
    return ERR_OK;
}

void mqtt5_client_delete_user_property(mqtt5_user_property_handle_t user_property)
{
    if (user_property) {
        mqtt5_user_property_item_t item, tmp;
        STAILQ_FOREACH_SAFE(item, user_property, next, tmp) {
            STAILQ_REMOVE(user_property, item, mqtt5_user_property, next);
            sys_mfree(item->key);
            sys_mfree(item->value);
            sys_mfree(item);
        }
        sys_mfree(user_property);
    }

}

bool mqtt5_set_if_config(char const *const new_config, char **old_config)
{
    if (new_config) {
        sys_mfree(*old_config);
        *old_config = string_dump(new_config);
        if (*old_config == NULL) {
            return false;
        }
    }
    return true;
}

static void mqtt5_client_delete_topic_alias(mqtt5_topic_alias_handle_t topic_alias_handle)
{
    if (topic_alias_handle) {
        mqtt5_topic_alias_item_t item, tmp;
        STAILQ_FOREACH_SAFE(item, topic_alias_handle, next, tmp) {
            STAILQ_REMOVE(topic_alias_handle, item, mqtt5_topic_alias, next);
            sys_mfree(item->topic);
            sys_mfree(item);
        }
        sys_mfree(topic_alias_handle);
    }
}

void mqtt5_user_info_config_mem_free(mqtt_client_t *client)
{
    if (client == NULL) {
        return;
    }

    if (client->mqtt5_config) {
        sys_mfree(client->mqtt5_config->will_property_info.content_type);
        sys_mfree(client->mqtt5_config->will_property_info.response_topic);
        sys_mfree(client->mqtt5_config->will_property_info.correlation_data);
        sys_mfree(client->mqtt5_config->server_resp_property_info.response_info);

        mqtt5_client_delete_topic_alias(client->mqtt5_config->peer_topic_alias);
        mqtt5_client_delete_user_property(client->mqtt5_config->connect_property_info.user_property);
        mqtt5_client_delete_user_property(client->mqtt5_config->will_property_info.user_property);
        mqtt5_client_delete_user_property(client->mqtt5_config->disconnect_property_info.user_property);

        sys_mfree(client->mqtt5_config);
    }

    return;
}

int mqtt5_client_set_connect_property(mqtt_client_t *client, const mqtt5_connection_property_config_t *connect_property)
{
    if (!client) {
        app_print("Client was not initialized\r\n");
        return ERR_ARG;
    }
    client->mqtt5_config = sys_calloc(1, sizeof(mqtt5_config_storage_t));
    MQTT5_MEM_CHECK(client->mqtt5_config, return ERR_MEM);
    mqtt5_config_storage_t *config = client->mqtt5_config;

    if (connect_property) {
        if (connect_property->session_expiry_interval) {
            config->connect_property_info.session_expiry_interval = connect_property->session_expiry_interval;
        }
        if (connect_property->maximum_packet_size) {
            if (connect_property->maximum_packet_size > 1024) {
                app_print("Connect maximum_packet_size property is over buffer_size(%d), Please first change it\r\n", MQTT_VAR_HEADER_BUFFER_LEN);
                return ERR_ARG;
            } else {
                config->connect_property_info.maximum_packet_size = connect_property->maximum_packet_size;
            }
        } else {
            config->connect_property_info.maximum_packet_size = 1024;
        }
        if (connect_property->receive_maximum) {
            config->connect_property_info.receive_maximum = connect_property->receive_maximum;
        }
        if (connect_property->topic_alias_maximum) {
            config->connect_property_info.topic_alias_maximum = connect_property->topic_alias_maximum;
            if (!config->peer_topic_alias) {
                config->peer_topic_alias = sys_calloc(1, sizeof(struct mqtt5_topic_alias_list_t));
                if (!(config->peer_topic_alias)) {
                    goto _mqtt_set_config_failed;
                }
                STAILQ_INIT(config->peer_topic_alias);
            }
        }

        if (connect_property->request_resp_info) {
            config->connect_property_info.request_resp_info = connect_property->request_resp_info;
        }
        if (connect_property->request_problem_info) {
            config->connect_property_info.request_problem_info = connect_property->request_problem_info;
        }
        if (connect_property->user_property) {
            mqtt5_client_delete_user_property(config->connect_property_info.user_property);
            config->connect_property_info.user_property = sys_calloc(1, sizeof(struct mqtt5_user_property_list_t));
            if (!(config->connect_property_info.user_property)) {
                goto _mqtt_set_config_failed;
            }
            STAILQ_INIT(config->connect_property_info.user_property);
            if (mqtt5_user_property_copy(config->connect_property_info.user_property, connect_property->user_property) != ERR_OK) {
                app_print("mqtt5_user_property_copy fail\r\n");
                goto _mqtt_set_config_failed;
            }
        }

        if (connect_property->payload_format_indicator) {
            config->will_property_info.payload_format_indicator = connect_property->payload_format_indicator;
        }
        if (connect_property->will_delay_interval) {
            config->will_property_info.will_delay_interval = connect_property->will_delay_interval;
        }
        if (connect_property->message_expiry_interval) {
            config->will_property_info.message_expiry_interval = connect_property->message_expiry_interval;
        }
        MQTT5_MEM_CHECK(mqtt5_set_if_config(connect_property->content_type, &config->will_property_info.content_type), goto _mqtt_set_config_failed);
        MQTT5_MEM_CHECK(mqtt5_set_if_config(connect_property->response_topic, &config->will_property_info.response_topic), goto _mqtt_set_config_failed);
        if (connect_property->correlation_data && connect_property->correlation_data_len) {
            sys_mfree(config->will_property_info.correlation_data);
            config->will_property_info.correlation_data = sys_calloc(1, connect_property->correlation_data_len);
            MQTT5_MEM_CHECK(config->will_property_info.correlation_data, goto _mqtt_set_config_failed);
            memcpy(config->will_property_info.correlation_data, connect_property->correlation_data, connect_property->correlation_data_len);
            config->will_property_info.correlation_data_len = connect_property->correlation_data_len;
        }

        if (connect_property->will_user_property) {
            mqtt5_client_delete_user_property(config->will_property_info.user_property);
            config->will_property_info.user_property = sys_calloc(1, sizeof(struct mqtt5_user_property_list_t));
            MQTT5_MEM_CHECK(config->will_property_info.user_property, goto _mqtt_set_config_failed);
            STAILQ_INIT(config->will_property_info.user_property);
            if (mqtt5_user_property_copy(config->will_property_info.user_property, connect_property->will_user_property) != ERR_OK) {
                app_print("mqtt5_user_property_copy fail\r\n");
                goto _mqtt_set_config_failed;
            }
        }
    }

    return ERR_OK;
_mqtt_set_config_failed:
    mqtt5_user_info_config_mem_free(client);
    return ERR_MEM;
}

int mqtt5_param_cfg(mqtt_client_t *mqtt_client)
{
    if (mqtt_client == NULL) {
        return -1;
    }

    if ((mqtt5_client_set_user_property(&connect_property.user_property, user_property_arr, 3) != 0) ||
        (mqtt5_client_set_connect_property(mqtt_client, &connect_property) != 0)) {
        app_print("user info init failed!\r\n");

        return -1;
    }

    mqtt_client->mqtt5_config->publish_property_info = &publish_property;
    mqtt_client->mqtt5_config->server_resp_property_info.response_info = NULL;
    mqtt_client->mqtt5_config->subscribe_property_info = &subscrible_property;
    mqtt_client->mqtt5_config->unsubscribe_property_info = &unsubscrible_property;
    mqtt_client->mqtt5_config->disconnect_property_info.user_property =  disconnect_property.user_property;
    mqtt_client->mqtt5_config->disconnect_property_info.disconnect_reason = disconnect_property.disconnect_reason;
    mqtt_client->mqtt5_config->disconnect_property_info.user_property = disconnect_property.user_property;

    return 0;
}

void mqtt5_param_delete(mqtt_client_t *mqtt_client)
{
    mqtt5_client_delete_user_property(connect_property.user_property);
    connect_property.user_property = 0;
    mqtt5_user_info_config_mem_free(mqtt_client);
    mqtt_client_free(mqtt_client);

    if (connect_property.content_type) {
        sys_mfree((void *)connect_property.content_type);
        connect_property.content_type = NULL;
    }
    if (connect_property.response_topic) {
        sys_mfree((void *)connect_property.response_topic);
        connect_property.response_topic = NULL;
    }
    if (connect_property.correlation_data) {
        sys_mfree((void *)connect_property.correlation_data);
        connect_property.correlation_data = NULL;
    }

    return;
}
#endif //CONFIG_MQTT
