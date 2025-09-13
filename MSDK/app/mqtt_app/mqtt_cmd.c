/*!
    \file    mqtt_cmd.c
    \brief   MQTT command shell for GD32VW55x SDK.

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

#include "app_cfg.h"

#ifdef CONFIG_MQTT
#include <stdbool.h>
#include "mqtt_cmd.h"
#include "cmd_shell.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/apps/mqtt5.h"
#include "wrapper_os.h"
#include "lwip/netdb.h"
#include "lwip/tcpip.h"

#include "mqtt_ssl_config.c"

#include "mqtt_client_config.c"

#include "mqtt5_client_config.c"

#define AUTO_RECONNECT_LIMIT    5
bool auto_reconnect = false;
uint8_t auto_reconnect_num = 0;
uint32_t auto_reconnect_interval = 20000; // ms, 20s

static bool mqtt_task_suspended;
static void *mqtt_task_handle = NULL;
static mqtt_client_t *mqtt_client = NULL;
struct mqtt_connect_client_info_t *client_user_info = NULL;

ip_addr_t sever_ip_addr = IPADDR4_INIT_BYTES(0, 0, 0, 0);
uint16_t port;
uint8_t tls_encry_mode = TLS_AUTH_MODE_NONE;

int16_t connect_fail_reason = -1;

typedef struct cmd_msg_pub
{
    struct co_list cmd_msg_pub_list;
} cmd_msg_pub_t;
static cmd_msg_pub_t msg_pub_list;

typedef struct cmd_msg_sub
{
    struct co_list cmd_msg_sub_list;
} cmd_msg_sub_t;
static cmd_msg_sub_t msg_sub_list;

enum mqtt_mode mqtt_cmd_mode = 0;
void mqtt_mode_type_set(enum mqtt_mode cmd_mode)
{
    mqtt_cmd_mode = cmd_mode;
}

enum mqtt_mode mqtt_mode_type_get(void)
{
    return mqtt_cmd_mode;
}

static publish_msg_t* publish_msg_mem_malloc(uint16_t input_topic_len, uint16_t input_msg_len)
{
    publish_msg_t *pub_msg = sys_calloc(1, sizeof(publish_msg_t));
    if (pub_msg == NULL) {
        return NULL;
    }

    pub_msg->topic = (char*)sys_malloc(input_topic_len);
    if (pub_msg->topic == NULL) {
        sys_mfree(pub_msg);
        return NULL;
    }

    pub_msg->msg = (char*)sys_malloc(input_msg_len);
    if (pub_msg->msg == NULL) {
        sys_mfree(pub_msg->topic);
        sys_mfree(pub_msg);
        return NULL;
    }
    return pub_msg;
}

static void publish_msg_mem_free(publish_msg_t *pub_msg)
{
    if (pub_msg == NULL) {
        return;
    }
    sys_mfree(pub_msg->topic);
    sys_mfree(pub_msg->msg);
    sys_mfree(pub_msg);
    return;
}

static sub_msg_t* sub_msg_mem_malloc(uint16_t input_topic_len)
{
    sub_msg_t *sub_msg = sys_calloc(1, sizeof(sub_msg_t));
    if (sub_msg == NULL) {
        return NULL;
    }

    sub_msg->topic = (char*)sys_malloc(input_topic_len);
    if (sub_msg->topic == NULL) {
        sys_mfree(sub_msg);
        return NULL;
    }

    return sub_msg;
}

static void sub_msg_mem_free(sub_msg_t *msg_p)
{
    if (msg_p == NULL) {
        return;
    }
    sys_mfree(msg_p->topic);
    sys_mfree(msg_p);
    return;
}

static err_t client_user_info_malloc(uint16_t user_name_len, uint16_t user_password_len)
{
    client_user_info->client_user = (char*)sys_malloc(user_name_len);
    if (client_user_info->client_user == NULL) {
        return ERR_MEM;
    }

    client_user_info->client_pass = (char*)sys_malloc(user_password_len);
    if (client_user_info->client_pass == NULL) {
        sys_mfree(client_user_info->client_user);
        return ERR_MEM;
    }
    return ERR_OK;
}

void mqtt_task_suspend(void)
{
    mqtt_task_suspended = true;
    sys_task_wait_notification(-1);
    return;
}

void mqtt_task_resume(bool isr)
{
    if (!mqtt_task_suspended) {
        return;
    }

    mqtt_task_suspended = false;
    sys_task_notify(mqtt_task_handle, isr);
    return;
}

static void mqtt_resource_free(void)
{
    mqtt_ssl_cfg_free(mqtt_client);
    client_user_info_free();
    mqtt5_param_delete(mqtt_client);
    mqtt_client_free(mqtt_client);
    mqtt_client = NULL;

    return;
}

void mqtt_publish_msg_handle(void)
{
    publish_msg_t *pub_msg = NULL;

    while (!co_list_is_empty(&(msg_pub_list.cmd_msg_pub_list))) {
        sys_sched_lock();
        pub_msg = (publish_msg_t *)co_list_pop_front(&(msg_pub_list.cmd_msg_pub_list));
        sys_sched_unlock();

        LOCK_TCPIP_CORE();
        if (mqtt_mode_type_get() == MODE_TYPE_MQTT5) {
            mqtt5_msg_publish(mqtt_client, pub_msg->topic, pub_msg->msg, strlen((const char *)pub_msg->msg), pub_msg->qos, pub_msg->retain,
                            mqtt_pub_cb, (void *)pub_msg, mqtt_client->mqtt5_config->publish_property_info,
                            mqtt_client->mqtt5_config->server_resp_property_info.response_info);
        } else {
            mqtt_msg_publish(mqtt_client, pub_msg->topic, pub_msg->msg, strlen((const char *)pub_msg->msg),
                            pub_msg->qos, pub_msg->retain, mqtt_pub_cb, (void *)pub_msg);
        }
        UNLOCK_TCPIP_CORE();
        publish_msg_mem_free(pub_msg);
    }
    return;
}

void mqtt_subscribe_or_unsubscribe_msg_handle(void)
{
    sub_msg_t *sub_msg = NULL;
    mqtt5_topic_t sub_list;

    while (!co_list_is_empty(&(msg_sub_list.cmd_msg_sub_list))) {
        sys_sched_lock();
        sub_msg = (sub_msg_t *)co_list_pop_front(&(msg_sub_list.cmd_msg_sub_list));
        sys_sched_unlock();

        LOCK_TCPIP_CORE();
        if (mqtt_mode_type_get() == MODE_TYPE_MQTT5) {
            if (sub_msg->sub_or_unsub) {
                sub_list.filter = sub_msg->topic;
                sub_list.qos = (int)sub_msg->qos;
                mqtt5_msg_subscribe(mqtt_client, mqtt_sub_cb, client_user_info, &sub_list,
                            1, mqtt_client->mqtt5_config->subscribe_property_info);
            } else {
                mqtt5_msg_unsub(mqtt_client, sub_msg->topic, sub_msg->qos, mqtt_unsub_cb,
                            client_user_info, mqtt_client->mqtt5_config->unsubscribe_property_info);
            }
        } else {
            void *mag_send_call_back = (sub_msg->sub_or_unsub) == true ? mqtt_sub_cb : mqtt_unsub_cb;
            mqtt_sub_unsub(mqtt_client, sub_msg->topic, sub_msg->qos, mag_send_call_back,
                            client_user_info, sub_msg->sub_or_unsub);
        }
        UNLOCK_TCPIP_CORE();
        sub_msg_mem_free(sub_msg);
    }
    return;
}

void mqtt_fail_reason_display(mqtt_connect_return_res_t fail_reason)
{
    char *prefix = "MQTT mqtt_client: connection refused reason is ";
    char *reason = NULL;

    switch(fail_reason) {
    case MQTT_CONNECTION_REFUSE_PROTOCOL:
        reason = "Bad protocol";
        break;
    case MQTT_CONNECTION_REFUSE_ID_REJECTED:
        reason = "ID rejected";
        break;
    case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
        reason = "Server unavailable";
        break;
    case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
        reason = "Bad username or password";
        break;
    case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
        reason = "Not authorized";
        break;

    default:
        reason = "Unknown reason";
        break;
    }
    app_print("%s%s, id is %d\r\n", prefix, reason, fail_reason);

    return;
}

void mqtt5_fail_reason_display(mqtt5_connect_return_res_t fail_reason)
{
    char *prefix = "MQTT mqtt_client: connection refused reason is ";
    char *reason = NULL;

    switch (fail_reason) {
    case MQTT5_UNSPECIFIED_ERROR:
        reason = "Unspecified error";
        break;
    case MQTT5_MALFORMED_PACKET:
        reason = "Malformed Packet";
        break;
    case MQTT5_PROTOCOL_ERROR:
        reason = "Protocol Error";
        break;
    case MQTT5_IMPLEMENT_SPECIFIC_ERROR:
        reason = "Implementation specific error";
        break;
    case MQTT5_UNSUPPORTED_PROTOCOL_VER:
        reason = "Unsupported Protocol Version";
        break;
    case MQTT5_INVAILD_CLIENT_ID:
        reason = "Client Identifier not valid";
        break;
    case MQTT5_BAD_USERNAME_OR_PWD:
        reason = "Bad User Name or Password";
        break;
    case MQTT5_NOT_AUTHORIZED:
        reason = "Not authorized";
        break;
    case MQTT5_SERVER_UNAVAILABLE:
        reason = "Server unavailable";
        break;
    case MQTT5_SERVER_BUSY:
        reason = "Server busy";
        break;
    case MQTT5_BANNED:
        reason = "Banned";
        break;
    case MQTT5_SERVER_SHUTTING_DOWN:
        reason = "Server shutting down";
        break;
    case MQTT5_BAD_AUTH_METHOD:
        reason = "Bad authentication method";
        break;
    case MQTT5_KEEP_ALIVE_TIMEOUT:
        reason = "Keep Alive timeout";
        break;
    case MQTT5_SESSION_TAKEN_OVER:
        reason = "Session taken over";
        break;
    case MQTT5_TOPIC_FILTER_INVAILD:
        reason = "Topic Filter invalid";
        break;
    case MQTT5_TOPIC_NAME_INVAILD:
        reason = "Topic Name invalid";
        break;
    case MQTT5_PACKET_IDENTIFIER_IN_USE:
        reason = "Packet Identifier in use";
        break;
    case MQTT5_PACKET_IDENTIFIER_NOT_FOUND:
        reason = "Packet Identifier not found";
        break;
    case MQTT5_RECEIVE_MAXIMUM_EXCEEDED:
        reason = "Receive Maximum exceeded";
        break;
    case MQTT5_TOPIC_ALIAS_INVAILD:
        reason = "Topic Alias invalid";
        break;
    case MQTT5_PACKET_TOO_LARGE:
        reason = "Packet too large";
        break;
    case MQTT5_MESSAGE_RATE_TOO_HIGH:
        reason = "Message rate too high";
        break;
    case MQTT5_QUOTA_EXCEEDED:
        reason = "Quota exceeded";
        break;
    case MQTT5_ADMINISTRATIVE_ACTION:
        reason = "Administrative action";
        break;
    case MQTT5_PAYLOAD_FORMAT_INVAILD:
        reason = "Payload format invalid";
        break;
    case MQTT5_RETAIN_NOT_SUPPORT:
        reason = "Retain not supported";
        break;
    case MQTT5_QOS_NOT_SUPPORT:
        reason = "QoS not supported";
        break;
    case MQTT5_USE_ANOTHER_SERVER:
        reason = "Use another server";
        break;
    case MQTT5_SERVER_MOVED:
        reason = "Server moved";
        break;
    case MQTT5_SHARED_SUBSCR_NOT_SUPPORTED:
        reason = "Shared Subscriptions not supported";
        break;
    case MQTT5_CONNECTION_RATE_EXCEEDED:
        reason = "Connection rate exceeded";
        break;
    case MQTT5_MAXIMUM_CONNECT_TIME:
        reason = "Maximum connect time";
        break;
    case MQTT5_SUBSCRIBE_IDENTIFIER_NOT_SUPPORT:
        reason = "Subscription Identifiers not supported";
        break;
    case MQTT5_WILDCARD_SUBSCRIBE_NOT_SUPPORT:
        reason = "Wildcard Subscriptions not supported";
        break;

    default:
        reason = "Unknown reason";
        break;
    }
    app_print("%s%s, id is %d\r\n", prefix, reason, fail_reason);

    return;
}

void mqtt_connect_severy_fail_reason_display(int16_t fail_reason)
{
    if (mqtt_mode_type_get() == MODE_TYPE_MQTT5) {
        mqtt5_fail_reason_display((mqtt5_connect_return_res_t)fail_reason);
    } else {
        mqtt_fail_reason_display((mqtt_connect_return_res_t)fail_reason);
    }

    return;
}

int16_t mqtt_connect_to_server(void)
{
    uint32_t connect_time = sys_current_time_get();

    connect_fail_reason = -1;
    app_print("\r\n");
    app_print("MQTT: Linking server...\r\n");

    LOCK_TCPIP_CORE();
    if (mqtt_mode_type_get() == MODE_TYPE_MQTT5) {
        if (mqtt5_client_connect(mqtt_client, &sever_ip_addr, port, mqtt_connect_callback, NULL, client_user_info,
                                              &(mqtt_client->mqtt5_config->connect_property_info),
                                              &(mqtt_client->mqtt5_config->will_property_info)) != ERR_OK) {
            app_print("MQTT mqtt_client: connect to server failed\r\n");
            UNLOCK_TCPIP_CORE();
            return connect_fail_reason;
        }
    } else {
        if (mqtt_client_connect(mqtt_client, &sever_ip_addr, port, mqtt_connect_callback, NULL, client_user_info) != ERR_OK) {
            app_print("MQTT mqtt_client: connect to server failed\r\n");
            UNLOCK_TCPIP_CORE();
            return connect_fail_reason;
        }
    }
    UNLOCK_TCPIP_CORE();

    mqtt_set_inpub_callback(mqtt_client, mqtt_receive_pub_msg_print, mqtt_receive_msg_print, client_user_info);
    mqtt_task_handle = sys_current_task_handle_get();

    while(mqtt_client_is_connected(mqtt_client) == false) {
        if (sys_current_time_get() - connect_time > MQTT_LINK_TIME_LIMIT) {
            app_print("MQTT: Connection timed out\r\n");
            return -1;
        }
        if ((mqtt_mode_type_get() == MODE_TYPE_MQTT5) && (connect_fail_reason == MQTT_CONNECTION_REFUSE_PROTOCOL)) {
            LOCK_TCPIP_CORE();
            mqtt5_disconnect(mqtt_client);
            UNLOCK_TCPIP_CORE();
            mqtt5_param_delete(mqtt_client);
            mqtt_mode_type_set(MODE_TYPE_MQTT);
            app_print("MQTT: The server does not support version 5.0, now switch to version 3.1.1\r\n");
            return mqtt_connect_to_server();
        } else if (connect_fail_reason > 0) {
            mqtt_connect_severy_fail_reason_display(connect_fail_reason);
            return connect_fail_reason;
        }
    }

    app_print("MQTT: Successfully connected to server\r\n");
    app_print("# ");
    mqtt_client->run = true;
    auto_reconnect_num = 0;

    return 0;
}

void mqtt_connect_free(void)
{
    LOCK_TCPIP_CORE();
    if (mqtt_mode_type_get() == MODE_TYPE_MQTT5) {
        mqtt5_disconnect(mqtt_client);
    } else {
        mqtt_disconnect(mqtt_client);
    }
    UNLOCK_TCPIP_CORE();
    connect_fail_reason = -1;
    app_print("MQTT: disconnect with server\r\n");

    return;
}

static void mqtt_task(void *param)
{
    if (mqtt5_param_cfg(mqtt_client)) {
        app_print("MQTT: Configuration parameters failed, stop connection\r\n");
        goto exit;
    }
    if (mqtt_ssl_cfg(mqtt_client, tls_encry_mode)) {
        goto exit;
    }

    mqtt_client->run = false;
connect:
    mqtt_connect_to_server();

    while (mqtt_client->run) {
        mqtt_publish_msg_handle();
        mqtt_subscribe_or_unsubscribe_msg_handle();

        if (mqtt_client_is_connected(mqtt_client) == false) {
            if (auto_reconnect && auto_reconnect_num < AUTO_RECONNECT_LIMIT) {
                if (auto_reconnect_num)
                    sys_ms_sleep(auto_reconnect_interval * auto_reconnect_num);
                auto_reconnect_num++;
                goto connect;
            } else {
                break;
            }
        }
        mqtt_task_suspend();
    }

    mqtt_connect_free();

exit:
    mqtt_resource_free();
    sys_task_delete(NULL);

    return;
}

int rtos_mqtt_task_create(void)
{
    mqtt_task_suspended = false;
    os_task_t task_handle = sys_task_create_dynamic((const uint8_t *)"MQTT task",
                                        MQTT_TASK_STACK_SIZE, MQTT_TASK_PRIO, (task_func_t)mqtt_task, NULL);
    if (task_handle == NULL) {
        return -1;
    }
    return 0;
}

static int mqtt_ip_prase(ip_addr_t *addr_ip, char *domain)
{
    int i;
    uint16_t addr[4] = {0};
    struct addrinfo hints, *res;
    void *ptr;
    char ip_addr[32];

    if (domain == NULL) {
        goto err_ip_addr;
    }

    memset(&hints, 0, sizeof(hints));
    if (getaddrinfo(domain, NULL, &hints, &res) != 0) {
        goto err_ip_addr;
    }
    if (res->ai_family != AF_INET) {
        app_print("MQTT: only support ipv4 address.\r\n");
        freeaddrinfo(res);
        goto err_ip_addr;
    }
    ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;

    inet_ntop(res->ai_family, ptr, ip_addr, sizeof(ip_addr));
    freeaddrinfo(res);

    char *p = ip_addr;
    for (i = 0; i < 4; i++) {
        do {
            addr[i] = 10 * addr[i] + (uint16_t)(*p - '0');
            p++;
        } while(*p != '.' && *p != 0);
        p++;
        if (addr[i] > 255) {
            goto err_ip_addr;
        }
    }
#ifdef CONFIG_IPV6_SUPPORT
    addr_ip->u_addr.ip4.addr = PP_HTONL(LWIP_MAKEU32(addr[0], addr[1], addr[2], addr[3]));
#else
    addr_ip->addr= PP_HTONL(LWIP_MAKEU32(addr[0], addr[1], addr[2], addr[3]));
#endif

    return 0;

err_ip_addr:
    app_print("MQTT: error ip address\r\n");
    return -1;
}

void mqtt_client_info_set(int argc, char **argv)
{
    uint32_t len;
    char *clinet_id = mqtt_client_id_get();

    if (argc == 2) {
        app_print("MQTT: client id is: %s\r\n", clinet_id);
    } else if (argc == 3) {
        if (*(argv[2]) == '?') {
            goto usage;
        }
        len = strlen(argv[2]);
        if (len > 20) {
            app_print("MQTT: client id len must <= 20\r\n");
            return;
        }

        app_print("MQTT: old client id is %s\r\n", clinet_id);
        if (mqtt_client_id_set(argv[2], strlen(argv[2])) == 0) {
            app_print("MQTT: new client id is %s\r\n", clinet_id);
        } else {
            app_print("MQTT: client id set failed\r\n");
            goto usage;
        }
    } else {
        goto usage;
    }

    return;
usage:
    app_print("MQTT Usage: mqtt client_id [new client id]\r\n");
    return;
}

void mqtt_connect_server(int argc, char **argv)
{
    /*# if use certificates to prove identity, put ca, client_crt and client_private_key
     *  in mqtt_ssl_config.c which provided by the server.
     *# If access server whitout SSL, just undefine LWIP_SSL_MQTT in lwipopts.h.
    */
    uint16_t name_len, pass_len;

    if (mqtt_client != NULL) {
        app_print("MQTT: mqtt client is running, plese disconnect with the server first\r\n");
        return;
    }

    if ((argc == 3) && (*(argv[2]) == '?')) {
        goto usage;
    }
    if (argc < 5 || argc > 7) {
        goto usage;
    }

    mqtt_mode_type_set(MODE_TYPE_MQTT5);

    mqtt_client = mqtt_client_new();
    if (mqtt_client == NULL) {
        app_print("MQTT mqtt_client: rtos malloc mqtt client memory fail\r\n");
        return;
    }
    client_user_info = get_client_param_data_get();
    client_user_info->client_user = NULL;
    client_user_info->client_pass = NULL;
    port = MQTT_DEFAULT_PORT;

    if (mqtt_ip_prase(&sever_ip_addr, argv[2]) != 0) {
        app_print("MQTT mqtt_client: ip addrress error\r\n");
        goto exit;
    }

    port = (uint16_t)atoi(argv[3]);

    tls_encry_mode = (uint8_t)atoi(argv[4]);
    if (tls_encry_mode > 3) {
        app_print("MQTT mqtt_client: encryption set error\r\n");
        goto exit;
    }

    if (argc == 7) {
        name_len = strlen((const char *)argv[5]) + 1;
        pass_len = strlen((const char *)argv[6]) + 1;
        if (client_user_info_malloc(name_len, pass_len) != ERR_OK) {
            app_print("MQTT mqtt_client_user_info: rtos malloc client_user_info fail\r\n");
            goto exit;
        }
        memcpy(client_user_info->client_user, argv[5], name_len);
        memcpy(client_user_info->client_pass, argv[6], pass_len);
    }

    if(rtos_mqtt_task_create()) {
        app_print("MQTT mqtt_client: start mqtt task fail\r\n");
        client_user_info_free();
        mqtt_client_free(mqtt_client);
        mqtt_client = NULL;
    }
    return;

exit:
    mqtt_client_free(mqtt_client);
    mqtt_client = NULL;
usage:
    app_print("MQTT Usage: mqtt connect <server_ip> <server_port default:1883> <encryption: 0-3> [<user_name> <user_password>]\r\n");
    app_print("                 encryption: 0-no encryption; 1-TLS without pre-shared key and certificate;\r\n");
    app_print("                 encryption: 2-TLS with one-way certificate; 3-TLS with two-way certificate;\r\n");
    app_print("  # Use user_name and user_password which have be registered on the server to prove identity.\r\n");
    app_print("eg: mqtt connect 192.168.3.101 8885 2 vic 123\r\n");
    return;
}

void mqtt_msg_pub(int argc, char **argv)
{
    publish_msg_t *cmd_msg_pub = NULL;
    uint16_t input_topic_len, input_msg_len;

    if ((argc < 5) || (argc > 6)) {
        goto usage;
    }
    if ((argc == 3) && (*(argv[2]) == '?')) {
        goto usage;
    }

    if ((mqtt_client == NULL) || mqtt_client_is_connected(mqtt_client) == false) {
        app_print("MQTT mqtt_msg_pub: client is disconnected, please connect it\r\n");
        if (auto_reconnect != true) {
            return;
        }
    }

    input_topic_len = strlen((const char *)argv[2]) + 1;
    input_msg_len   = strlen((const char *)argv[3]) + 1;
    cmd_msg_pub     = publish_msg_mem_malloc(input_topic_len, input_msg_len);
    if (cmd_msg_pub == NULL) {
        app_print("MQTT mqtt_msg_pub: rtos malloc publish msg fail\r\n");
        return;
    }
    memcpy(cmd_msg_pub->topic, argv[2], input_topic_len);
    memcpy(cmd_msg_pub->msg,   argv[3], input_msg_len);

    cmd_msg_pub->qos = (uint8_t)atoi(argv[4]);
    if (cmd_msg_pub->qos > 2) {
        goto usage;
    }

    if (argc == 6) {
        if ((uint8_t)atoi(argv[5]) < 2) {
            cmd_msg_pub->retain = (uint8_t)atoi(argv[5]);
        } else {
            goto usage;
        }
    }

    sys_sched_lock();
    co_list_push_back(&(msg_pub_list.cmd_msg_pub_list), &(cmd_msg_pub->hdr));
    sys_sched_unlock();
    mqtt_task_resume(false);
    return;

usage:
    app_print("MQTT Usage: mqtt publish <topic_name> <topic_content> <qos: 0~2> [retain: 0/1]\r\n");
    app_print("     qos 0: The receiver receives the massage at most once\r\n");
    app_print("     qos 1: The receiver receives the massage at least once\r\n");
    app_print("     qos 2: The receiver receives the massage just once\r\n");
    app_print("     retain 0: not retain the topic in server\r\n");
    app_print("     retain 1: retain the topic in server for send to subscriber in the future\r\n");

    if (cmd_msg_pub)
        publish_msg_mem_free(cmd_msg_pub);
    return;
}

void mqtt_msg_sub(int argc, char **argv)
{
    sub_msg_t *cmd_msg_sub = NULL;
    uint16_t input_topic_len;

    if ((argc < 5) || (argc > 6)) {
        goto usage;
    }
    if ((argc == 3) && (*(argv[2]) == '?')) {
        goto usage;
    }

    if ((mqtt_client == NULL) || mqtt_client_is_connected(mqtt_client) == false) {
        app_print("MQTT mqtt_msg_sub: client is disconnected, please connect it\r\n");
        if (auto_reconnect != true) {
            return;
        }
    }

    input_topic_len = strlen((const char *)argv[2]) + 1;
    cmd_msg_sub = sub_msg_mem_malloc(input_topic_len);
    if (cmd_msg_sub == NULL) {
        app_print("MQTT mqtt_msg_sub: rtos malloc publish msg fail\r\n");
        return;
    }
    memcpy(cmd_msg_sub->topic, argv[2], input_topic_len);

    cmd_msg_sub->qos = (uint8_t)atoi(argv[3]);
    if (cmd_msg_sub->qos > 2) {
        goto usage;
    }

    if ((uint8_t)atoi(argv[4]) != 0) {
        cmd_msg_sub->sub_or_unsub = true;
    } else {
        cmd_msg_sub->sub_or_unsub = false;
    }

    sys_sched_lock();
    co_list_push_back(&(msg_sub_list.cmd_msg_sub_list), &(cmd_msg_sub->hdr));
    sys_sched_unlock();
    mqtt_task_resume(false);
    return;

usage:
    app_print("MQTT Usage: mqtt subscribe <topic_name> <qos: 0~2> <sub_or_unsub: 0/1>\r\n");
    app_print("     qos 0: The receiver receives the massage at most once\r\n");
    app_print("     qos 1: The receiver receives the massage at least once\r\n");
    app_print("     qos 2: The receiver receives the massage just once\r\n");
    app_print("     sub_or_unsub 0: unsubscribe the topic \r\n");
    app_print("     sub_or_unsub 1: subscribe the topic \r\n");

    if (cmd_msg_sub)
        sub_msg_mem_free(cmd_msg_sub);
    return;
}

void mqtt_auto_reconnect_set(int argc, char **argv)
{
    if (argc == 2) {
        app_print("MQTT: current auto reconnect = %d\r\n", auto_reconnect);
    } else if (argc == 3) {
        if (*(argv[2]) == '?') {
            goto usage;
        }
        uint8_t get = (uint16_t)atoi(argv[2]);
        if (get > 0) {
            get = 1;
        }

        app_print("MQTT: current auto reconnect = %d, then auto reconnect = %d\r\n", auto_reconnect, get);
        auto_reconnect = (bool)get;
    } else {
        goto usage;
    }

    return;
usage:
    app_print("MQTT Usage: mqtt auto_reconnect [0: disable; 1: enable]\r\n");
    return;
}

void mqtt_client_disconnect(int argc, char **argv)
{
    if (mqtt_client != NULL) {
        mqtt_client->run = false;
    }

    mqtt_task_resume(false);
    return;
}

void cmd_mqtt(int argc, char **argv)
{
    /**
     * Test with mosquitto Server or Alibaba Cloud or baidu AI cloud(mqtt 3.1.1)
     * For mosquitto Server; execute CMD: mosquitto.exe -c mosquitto.conf to start svrver
     * in the server installation directory.
     * All server related configurations are in the mosquitto.conf file.
    */
    if (argc <= 1) {
        goto usage;
    }

    if (strcmp(argv[1], "connect") == 0) {
        mqtt_connect_server(argc, argv);
    } else if (strcmp(argv[1], "publish") == 0) {
        mqtt_msg_pub(argc, argv);
    } else if (strcmp(argv[1], "subscribe") == 0) {
        mqtt_msg_sub(argc, argv);
    } else if (strcmp(argv[1], "disconnect") == 0) {
        mqtt_client_disconnect(argc, argv);
    } else if (strcmp(argv[1], "auto_reconnect") == 0) {
        mqtt_auto_reconnect_set(argc, argv);
    } else if (strcmp(argv[1], "client_id") == 0) {
        mqtt_client_info_set(argc, argv);
    } else if (strcmp(argv[1], "help") == 0) {
        goto usage;
    } else {
        app_print("MQTT: mqtt command error\r\n");
    }

    return;

usage:
    app_print("Usage: \r\n");
    app_print("    mqtt <connect | publish | subscribe | help | ...> [param0] [param1]...\r\n");
    app_print("         connect <server_ip> <server_port default:1883> <encryption: 0-3> [<user_name> <user_password>]\r\n");
    app_print("                 encryption: 0-no encryption; 1-TLS without pre-shared key and certificate;\r\n");
    app_print("                 encryption: 2-TLS with one-way certificate; 3-TLS with two-way certificate;\r\n");
    app_print("         publish <topic_name> <topic_content> <qos: 0~2> [retain: 0/1]\r\n");
    app_print("         subscribe  <topic_name> <qos: 0~2> <sub_or_unsub: 0/1 0 q is sub; 0 is unsub>\r\n");
    app_print("         disconnect               --disconnect with server\r\n");
    app_print("         auto_reconnect           --set auto reconnect to server\r\n");
    app_print("         client_id [gigadevice2]  --check or change client_id\r\n");
    app_print("eg1.\r\n");
    app_print("    mqtt connect 192.168.3.101 8885 2 vic 123\r\n");
    app_print("eg2.\r\n");
    app_print("    mqtt publish topic helloworld 1 0\r\n");
    app_print("eg3.\r\n");
    app_print("    mqtt subscribe topic 0 1\r\n");
    app_print("eg4.\r\n");
    app_print("    mqtt subscribe ?\r\n");

    return;
}

#endif //CONFIG_MQTT
