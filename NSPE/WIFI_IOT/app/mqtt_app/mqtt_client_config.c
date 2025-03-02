/*!
    \file    mqtt_client_config.c
    \brief   MQTT client data handle for GD32W51x WiFi SDK

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
#include "mqtt_client_config.h"
#include "wrapper_os.h"

char client_id[] = {'G','i','g','a','D','e','v','i','c','e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct mqtt_connect_client_info_t base_client_user_info = {
    //client_id
    client_id,
    //client_user
    NULL,
    //client_password
    NULL,
    //keep_alive
    300,
    //topic
    NULL,
    //msg
    NULL,
    //qos
    0,
    //retain
    0
};

int mqtt_client_id_set(char *new_client_id, int16_t len)
{
    if (new_client_id == NULL) {
        app_print("client id is NULL\r\n");
        return -1;
    }

    if (len >= ARRAY_SIZE(client_id)) {
        app_print("name is too long\r\n");
        return -2;
    }
    sys_memcpy(client_id, new_client_id, len);

    return 0;
}

void mqtt_pub_cb( void *arg, err_t status)
{
    if (status == ERR_OK) {
        app_print("massage publish success\r\n");
    } else if (status == ERR_TIMEOUT) {
        app_print("massage publish time out\r\n");
    }
    app_print("# \r\n");

    return;
}

void mqtt_sub_cb(void *arg, err_t status)
{
    if (status == ERR_OK) {
        app_print("massage subscribe success\r\n");
    } else if (status == ERR_TIMEOUT) {
        app_print("massage subscribe time out\r\n");
    }
    app_print("# \r\n");

    return;
}

void mqtt_unsub_cb(void *arg, err_t status)
{
    if (status == ERR_OK) {
        app_print("massage unsubscribe success\r\n");
    } else if (status == ERR_TIMEOUT) {
        app_print("massage unsubscribe time out\r\n");
    }
    app_print("# \r\n");

    return;
}

void mqtt_receive_msg_print(void *inpub_arg, const uint8_t *data, uint16_t payload_length, uint8_t flags, uint8_t retain)
{

    if (retain > 0 ) {
        app_print("retain: ");
    }

    app_print("payload: ");
    for (int idx = 0; idx < payload_length; idx++)
    {
        app_print("%c", *data);
        data++;
    }

    app_print("\r\n");

    return;
}

void mqtt_receive_pub_msg_print(void *inpub_arg, const char *data, uint32_t payload_length)
{
    app_print("receiced topic: ");
    for (int idx = 0; idx < payload_length; idx++)
    {
        app_print("%c", *data);
        data++;
    }
    app_print("  ");

    return;
}

void mqtt_connect_callback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    char *prefix = NULL;
    char *reason = NULL;

    if ((status == MQTT_CONNECT_ACCEPTED) ||
        (status == MQTT_CONNECT_REFUSED_PROTOCOL_VERSION)) {
            return;
    }

    prefix = "MQTT: client will be closed, reason is ";
    switch (status) {
        case MQTT_CONNECT_DISCONNECTED:
            reason = "remote has closed connection";
            break;
        case MQTT_CONNECT_TIMEOUT:
            reason = "connect to server timed out";
            break;
        default:
            reason = "others";
            break;
    }

    app_print("%s%s, id is %d\r\n", prefix, reason, status);
    return;
}

struct mqtt_connect_client_info_t* get_client_param_data_get(void)
{
    return &base_client_user_info;
}

void client_user_info_free(void)
{
    if (base_client_user_info.client_user != NULL) {
        sys_mfree(base_client_user_info.client_user);
    }
    base_client_user_info.client_user = NULL;

    if (base_client_user_info.client_pass != NULL) {
        sys_mfree(base_client_user_info.client_pass);
    }
    base_client_user_info.client_pass = NULL;

    return;
}

#endif //CONFIG_MQTT
