/*!
    \file    mqtt_cmd.h
    \brief   mqtt command for GD32VW55x SDK.

    \version 2023-07-20, V1.0.0, firmware for GD32VW55x
*/

/*
    Copyright (c) 2023, GigaDevice Semiconductor Inc.

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

#ifndef _MQTT_CMD_H_
#define _MQTT_CMD_H_

#include "app_cfg.h"

#ifdef CONFIG_MQTT
#include "co_list.h"

#define MQTT_DEFAULT_PORT 1883

enum mqtt_mode {
    MODE_TYPE_MQTT = 1U,
    MODE_TYPE_MQTT5 = 2U,
};

typedef struct publish_msg {
    struct co_list_hdr hdr;
    char* topic;
    char* msg;
    uint8_t qos;
    uint8_t retain;
} publish_msg_t;

typedef struct sub_msg {
    struct co_list_hdr hdr;
    char* topic;
    uint8_t qos;
    bool sub_or_unsub;
} sub_msg_t;

void mqtt_mode_type_set(enum mqtt_mode cmd_mode);
enum mqtt_mode mqtt_mode_type_get(void);
void cmd_mqtt(int argc, char **argv);
void mqtt_connect_server(int argc, char **argv);
void mqtt_msg_pub(int argc, char **argv);
void mqtt_msg_sub(int argc, char **argv);
void mqtt_client_disconnect(int argc, char **argv);
void mqtt_auto_reconnect_set(int argc, char **argv);
void mqtt_task_resume(bool isr);

#endif

#endif // _MQTT_CMD_H_
