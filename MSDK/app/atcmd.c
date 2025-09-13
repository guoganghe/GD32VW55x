/*!
    \file    atcmd.c
    \brief   AT command for GD32VW55x SDK

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

#include "version.h"
#include "_build_date.h"
#include "app_cfg.h"
#include "wlan_config.h"
#include "build_config.h"
#include "gd32vw55x.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "wifi_vif.h"
#include "wifi_net_ip.h"
#include "wifi_management.h"
#include "wifi_export.h"
#include "wifi_init.h"
#include "cmd_shell.h"
#include "dbg_print.h"
#include "uart.h"
#include "uart_config.h"
#ifdef CONFIG_ATCMD_SPI
#include "spi.h"
#endif
#include "ping.h"
#include "net_iperf.h"
#include "log_uart.h"
#include "wakelock.h"
#include "atcmd.h"
#include "atcmd_wifi.h"
#include "atcmd_tcpip.h"
#ifdef CONFIG_AZURE_IOT_SUPPORT
#include "atcmd_azure.h"
#endif
#ifdef CFG_BLE_SUPPORT
#include "ble_config.h"
#include "ble_app_config.h"

#include "ble_init.h"
#include "ble_gatts.h"
#include "ble_gattc.h"
#include "ble_datatrans_srv.h"
#include "ble_datatrans_cli.h"
#include "app_dev_mgr.h"
#include "ble_adv.h"
#include "ble_scan.h"
#include "ble_error.h"
#include "app_adv_mgr.h"
#include "app_conn_mgr.h"
#include "app_adapter_mgr.h"
#include "app_per_sync_mgr.h"
#include "app_scan_mgr.h"
#include "ble_scan.h"
#include "ble_adapter.h"
#include "ble_gattc_co.h"
#include "app_sec_mgr.h"
#endif

#ifdef CONFIG_ATCMD
uart_config_t at_uart_conf = {
    .usart_periph = AT_UART,
    .baudrate = AT_UART_BAUDRATE,
    .databits = USART_WL_8BIT,
    .stopbits = USART_STB_1BIT,
    .parity = USART_PM_NONE,
    .flow_ctrl = (USART_CTS_DISABLE | USART_RTS_DISABLE)
};

static char at_hw_rx_buf[AT_HW_RX_BUF_SIZE];
static volatile uint16_t at_hw_rx_buf_idx = 0;
static os_sema_t at_hw_tx_sema = NULL;
static os_sema_t at_hw_dma_sema = NULL;
static os_sema_t at_ble_async_sema = NULL;
static volatile uint8_t at_cmd_received = 0;
static uint8_t at_task_exit = 0;
static const struct atcmd_entry atcmd_table[];

static void at_hw_dma_receive(uint32_t address, uint32_t num);
static void at_hw_send(char *data, int size);

#ifdef CONFIG_ATCMD_SPI
struct spi_manager_s spi_manager;
void at_spi_rcv_atcmd_config(bool from_isr);
static void at_spi_dma_receive_config(void);
static void at_spi_irq_receive_config(void);
static void at_spi_dma_receive_start(uint32_t address, uint32_t num);
static void at_spi_dma_receive_stop(void);
static void at_spi_dma_send_stop();

bool at_spi_rx_is_ongoing(void);
#else
static void at_uart_init(void);
static void at_uart_send(char *data, int size);
static void at_uart_dma_receive(uint32_t address, uint32_t num);

static void at_uart_dma_receive_config(void);
static void at_uart_irq_receive_config(void);
static void at_uart_dma_receive_start(uint32_t address, uint32_t num);
static void at_uart_dma_receive_stop(void);
static bool at_uart_rx_is_ongoing(void);
#endif
static void at_hw_dma_receive_config(void);
static void at_hw_irq_receive_config(void);
static void at_hw_dma_receive_start(uint32_t address, uint32_t num);
static void at_hw_dma_receive_stop(void);
static void at_hw_fill_rx_buf(char *p, uint8_t len);


/*!
    \brief      convert command to serial port configuration parameters
    \param[in]  cmd: serial port configuration parameters included in the command
    \param[out] gd: serial port configuration parameters
    \retval     none
*/
static void uart_param_at2gd(uart_config_t *cmd, uart_config_t *gd)
{
    gd->baudrate = cmd->baudrate;
    if (cmd->databits == 8)
        gd->databits = USART_WL_8BIT;
    else
        gd->databits = USART_WL_9BIT;

    if (cmd->stopbits == 0)
        gd->stopbits = USART_STB_0_5BIT;
    else if (cmd->stopbits == 1)
        gd->stopbits = USART_STB_1BIT;
    else if (cmd->stopbits == 2)
        gd->stopbits = USART_STB_1_5BIT;
    else
        gd->stopbits = USART_STB_2BIT;

    if (cmd->parity == 0)
        gd->parity = USART_PM_NONE;
    else if (cmd->parity == 1)
        gd->parity = USART_PM_ODD;
    else
        gd->parity = USART_PM_EVEN;

    if (cmd->flow_ctrl == 0)
        gd->flow_ctrl = 0;
    else if (cmd->flow_ctrl == 1)
        gd->flow_ctrl = USART_RTS_ENABLE;
    else if (cmd->flow_ctrl == 2)
        gd->flow_ctrl = USART_CTS_ENABLE;
    else
        gd->flow_ctrl = (USART_CTS_ENABLE | USART_RTS_ENABLE);
}

/*!
    \brief      convert serial port configuration parameters to command
    \param[in]  gd: serial port configuration parameters
    \param[out] cmd: serial port configuration parameters included in the command
    \retval     none
*/
static void uart_param_gd2at(uart_config_t *gd, uart_config_t *cmd)
{
    cmd->baudrate = gd->baudrate;
    if (gd->databits == USART_WL_8BIT)
        cmd->databits = 8;
    else
        cmd->databits = 9;
    if (gd->stopbits == USART_STB_0_5BIT)
        cmd->stopbits = 0;
    else if (gd->stopbits == USART_STB_1BIT)
        cmd->stopbits = 1;
    else if (gd->stopbits == USART_STB_1_5BIT)
        cmd->stopbits = 2;
    else
        cmd->stopbits = 3;
    if (gd->parity == USART_PM_NONE)
        cmd->parity = 0;
    else if (gd->parity == USART_PM_ODD)
        cmd->parity = 1;
    else
        cmd->parity = 2;
    cmd->flow_ctrl = 0;
    if ((gd->flow_ctrl & USART_RTS_ENABLE) == USART_RTS_ENABLE)
        cmd->flow_ctrl |= BIT(0);
    if ((gd->flow_ctrl & USART_CTS_ENABLE) == USART_CTS_ENABLE)
        cmd->flow_ctrl |= BIT(1);
}

int at_print(const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = print((void *)at_uart_conf.usart_periph, format, args, 0);
    va_end(args);

    return ret;
}

/*!
    \brief      parse string
    \param[in]  param: pointer to a string waiting to be parsed
    \param[out] none
    \retval     none
*/
char *at_string_parse(char *param)
{
    char *find = strchr(param, AT_QUOTE);
    char *head = NULL, *ptr, *tail = NULL;

    if (find == NULL)
        return NULL;
    head = ptr = ++find;
    while (1) {
        find = strchr(ptr, AT_QUOTE);
        if (find == NULL)
            break;
        else
            tail = find;
        ptr = find + 1;
    }
    if (tail != NULL)
        *tail = '\0';
    else
        return NULL;
    if (*head == '\0')
        head = NULL;
    return head;
}

/*!
    \brief      enter AT command mode
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_entry(int argc, char **argv)
{
    //cmd_mode_type_set(CMD_MODE_TYPE_AT);

    cip_info_init();

    AT_RSP_DIRECT("OK\r\n", 4);
}

/*!
    \brief      exit AT command mode
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_exit(int argc, char **argv)
{
    cip_info_reset();

    //cmd_mode_type_set(CMD_MODE_TYPE_NORMAL);

    AT_RSP_DIRECT("OK\r\n", 4);
}

/*!
    \brief      the AT command reset system
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_reset(int argc, char **argv)
{
    AT_RSP_DIRECT("OK\r\n", 4);
#ifdef CONFIG_ATCMD_SPI
    spi_tx_idle_wait();
#else
    uart_tx_idle_wait(at_uart_conf.usart_periph);
#endif
    rcu_deinit();
    SysTimer_SoftwareReset();
}

/*!
    \brief      the AT command show version of the software
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_version(int argc, char **argv)
{
    AT_RSP_START(256);
    AT_RSP("==================================\r\n");
    AT_RSP("SDK revsion: v%d.%d.%d\r\n", WIFI_VERSION_MAJ, WIFI_VERSION_MIN, WIFI_VERSION_REL);
#ifdef WIFI_GIT_REVISION
    AT_RSP("SDK git reversion: %s\r\n", WIFI_GIT_REVISION);
#endif
    AT_RSP("SDK build date: %s\r\n", SDK_BUILD_DATE);
    AT_RSP_OK();
}

/*!
    \brief      the AT command show all of the tasks
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_task(int argc, char **argv)
{
    AT_RSP_START(1024);
    sys_task_list(rsp_buf);
    rsp_buf_idx += strlen(rsp_buf);
    AT_RSP_OK();
}

/*!
    \brief      the AT command show heap usage
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_heap(int argc, char **argv)
{
    uint32_t free_sram = sys_free_heap_size();

    AT_RSP_START(256);
    AT_RSP("==================================\r\n");
    AT_RSP("Total free heap size = %d\r\n", free_sram);
    AT_RSP("Total min free heap size = %d\r\n", sys_min_free_heap_size());
    AT_RSP_OK();
}

/*!
    \brief      the AT command show system ram usage
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_sysram(int argc, char **argv)
{
    uint32_t free_sram = sys_free_heap_size();

    AT_RSP_START(256);
    AT_RSP("==================================\r\n");
    AT_RSP("Free SRAM size = %u\r\n", free_sram);
    AT_RSP_OK();
}

#ifndef CONFIG_ATCMD_SPI
/*!
    \brief      the AT command show the configuration of log usart or conifgurate log usart
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_uart(int argc, char **argv)
{
    uart_config_t cmd;
    AT_RSP_START(256);
    if (argc == 1) {
        if (argv[0][strlen(argv[0]) - 1] == AT_QUESTION) {
            uart_param_gd2at(&at_uart_conf, &cmd);
            AT_RSP("+UART: %d, %d, %d, %d, %d\r\n",
                cmd.baudrate, cmd.databits, cmd.stopbits, cmd.parity, cmd.flow_ctrl);
        } else {
            goto Error;
        }
    } else if (argc == 2) {
        if (argv[1][0] == AT_QUESTION)
            goto Usage;
        else
            goto Error;
    } else if (argc == 6) {
        char *endptr = NULL;
        cmd.baudrate = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        cmd.databits = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        cmd.stopbits = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        cmd.parity = (uint32_t)strtoul((const char *)argv[4], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        cmd.flow_ctrl = (uint32_t)strtoul((const char *)argv[5], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        if ((cmd.databits != 8 && cmd.databits != 9)
            || (cmd.stopbits > 3) || (cmd.parity > 2) || (cmd.flow_ctrl > 3))
            goto Error;

        uart_param_at2gd(&cmd, &at_uart_conf);
        at_uart_init();
    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;

Usage:
    AT_RSP("+UART=<baudrate>,<databits>,<stopbits>,<parity>,<flow control>\r\n");
    AT_RSP_OK();
    return;
}
#endif

/*!
    \brief      the AT command show AT command list
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void at_help(int argc, char **argv)
{
    int i;
    AT_RSP_START(2048);
    AT_RSP("\rAT COMMAND LIST:\r\n");
    AT_RSP("==============================\r\n");

    for (i = 0; atcmd_table[i].exec != NULL; i++) {
        AT_RSP("    %s\r\n", atcmd_table[i].name);
    }
    AT_RSP("\r\n");
    AT_RSP_OK();
}

#include "atcmd_tcpip.c"
#include "atcmd_wifi.c"
#ifdef CFG_BLE_SUPPORT
#include "..\ble\app\atcmd_ble.c"
#endif
#ifdef CONFIG_AZURE_F527_DEMO_SUPPORT
#ifndef CONFIG_ATCMD_SPI
#include "atcmd_azure.c"
#endif
#endif

static const struct atcmd_entry atcmd_table[] = {
    /* ====== Base ====== */
    {"AT", at_entry},
    {"ATQ", at_exit},
    {"AT+HELP", at_help},
    {"AT+RST", at_reset},
    {"AT+GMR", at_version},
    {"AT+TASK", at_task},
    {"AT+HEAP", at_heap},
    {"AT+SYSRAM", at_sysram},
#ifndef CONFIG_ATCMD_SPI
    {"AT+UART", at_uart},
#endif
    /* ====== WLAN ====== */
#ifdef CFG_WLAN_SUPPORT
    {"AT+CWMODE_CUR", at_cw_mode_cur},
    {"AT+CWJAP_CUR", at_cw_ap_cur_join},
    {"AT+CWLAP", at_cw_ap_list},
    {"AT+CWSTATUS", at_cw_status},
    {"AT+CWQAP", at_cw_ap_quit},
    {"AT+CWSAP_CUR", at_cw_ap_cur_start},
    {"AT+CWLIF", at_cw_ap_client_list},
    {"AT+CWAUTOCONN", at_cw_auto_connect},
    /* ====== TCPIP ====== */
    {"AT+PING", at_cip_ping},
    {"AT+CIPSTA", at_cip_sta_ip},
    {"AT+CIPSTART", at_cip_start},
    {"AT+CIPSEND", at_cip_send},
#ifdef CONFIG_ATCMD_SPI
    {"AT+CIPSDFILE", at_cip_send_file},
    {"AT+CIPRECVDATA", at_cip_recvdata},
#endif
    {"AT+CIPSERVER", at_cip_server},
    {"AT+CIPCLOSE", at_cip_close},
    {"AT+CIPSTATUS", at_cip_status},
    {"AT+CIPMODE", at_cip_mode},
    {"AT+TRANSINTVL", at_trans_interval},
    {"AT+CIFSR", at_cip_ip_addr_get},
#ifdef CONFIG_AZURE_F527_DEMO_SUPPORT
#ifndef CONFIG_ATCMD_SPI
    /* ====== Azure Cloud ====== */
    {"AT+AZCWJAP", at_azure_wifi_connect},
    {"AT+AZCOMC", at_azure_component_create},
    {"AT+AZCERT", at_azure_cert},
    {"AT+AZSYMKEY", at_azure_symkey},
    {"AT+AZEPT", at_azure_endpoint},
    {"AT+AZIDSP", at_azure_idsp},
    {"AT+AZDEVREGID", at_azure_regid},
    {"AT+AZPORT", at_azure_port},
    {"AT+AZPNPMODID", at_azure_pnp_modid},
    {"AT+AZDEVID", at_azure_devid},
    {"AT+AZHOSTNM", at_azure_host_name},
    {"AT+AZADUMANUF", at_azure_adu_manufacturer},
    {"AT+AZADUMOD", at_azure_adu_model},
    {"AT+AZADUPROV", at_azure_adu_provider},
    {"AT+AZADUPNM", at_azure_adu_updatename},
    {"AT+AZADUPVER", at_azure_adu_updatever},
    {"AT+AZCONN", at_azure_connect},
    {"AT+AZDISC", at_azure_disconnect},
    {"AT+AZTELS", at_azure_telemetry_update},
    {"AT+AZPROPS", at_azure_property_update},
    {"AT+AZPROPRSP", at_azure_property_rsp},
    {"AT+AZCMDRSP", at_azure_cmd_rsp},
    {"AT+AZSTAT", at_azure_state_get},
    //{"AT+AZOTARSP", at_azure_ota_rsp},
    {"AT+AZDEVUPT", at_azure_dev_update},  //directly update the value of slave device to azure
#endif //CONFIG_ATCMD_SPI
#endif //CONFIG_AZURE_F527_DEMO_SUPPORT
#endif //CFG_WLAN_SUPPORT

#ifdef CFG_BLE_SUPPORT
    {"AT+BLEENABLE", at_ble_enable},
    {"AT+BLEDISABLE", at_ble_disable},
    {"AT+BLENAME", at_ble_name},
    {"AT+BLEADDR", at_ble_bd_addr},
    {"AT+BLESETAUTH", at_ble_set_auth},
    {"AT+BLEPAIR", at_ble_pair},
    {"AT+BLEENCRYPT", at_ble_encrypt},
    {"AT+BLEPASSKEY", at_ble_passkey},
    {"AT+BLECOMPARE", at_ble_compare},
    {"AT+BLELISTENCDEV", at_ble_list_enc_dev},
    {"AT+BLECLEARENCDEV", at_ble_clear_enc_dev},

#if (BLE_CFG_ROLE & (BLE_CFG_ROLE_BROADCASTER | BLE_CFG_ROLE_PERIPHERAL))
    {"AT+BLEADVSTART", at_ble_adv_start},
    {"AT+BLEADVSTOP", at_ble_adv_stop},
    {"AT+BLEADVDATA", at_ble_adv_data},
    {"AT+BLEADVDATAEX", at_ble_adv_data_ex},
    {"AT+BLESCANRSPDATA", at_ble_scan_rsp_data},
    {"AT+BLEPASSTH", at_ble_passth_mode_enable},
    {"AT+BLEPASSTHAUTO", at_ble_passth_mode_auto_enable},
    {"AT+BLEGATTSSVC", at_ble_gatts_list_svc},
    {"AT+BLEGATTSCHAR", at_ble_gatts_list_char},
    {"AT+BLEGATTSDESC", at_ble_gatts_list_desc},
    {"AT+BLEGATTSLISTALL", at_ble_gatts_list_all},
    {"AT+BLEGATTSNTF", at_ble_gatts_ntf},
    {"AT+BLEGATTSIND", at_ble_gatts_ind},
    {"AT+BLEGATTSSETATTRVAL", at_ble_gatts_set_attr_val},
#endif // (BLE_CFG_ROLE & (BLE_CFG_ROLE_BROADCASTER | BLE_CFG_ROLE_PERIPHERAL))

#if (BLE_APP_GATT_CLIENT_SUPPORT)
    {"AT+BLEGATTCDISCSVC", at_ble_gattc_disc_svc},
    {"AT+BLEGATTCDISCCHAR", at_ble_gattc_disc_char},
    {"AT+BLEGATTCDISCDESC", at_ble_gattc_disc_desc},
    {"AT+BLEGATTCRD", at_ble_gattc_read},
    {"AT+BLEGATTCWR", at_ble_gattc_write},
    {"AT+BLEPASSTHCLI", at_ble_passth_cli_mode_enable},
#endif

#if (BLE_CFG_ROLE & (BLE_CFG_ROLE_OBSERVER | BLE_CFG_ROLE_CENTRAL))
    {"AT+BLESCANPARAM", at_ble_scan_param},
    {"AT+BLESCAN", at_ble_scan},

#if (BLE_APP_PER_ADV_SUPPORT)
    {"AT+BLESYNC", at_ble_sync},
    {"AT+BLESYNCSTOP", at_ble_sync_stop},
#endif // (BLE_APP_PER_ADV_SUPPORT)
#endif // (BLE_CFG_ROLE & (BLE_CFG_ROLE_OBSERVER | BLE_CFG_ROLE_CENTRAL))

#if (BLE_CFG_ROLE & (BLE_CFG_ROLE_PERIPHERAL | BLE_CFG_ROLE_CENTRAL))
#if (BLE_CFG_ROLE & BLE_CFG_ROLE_CENTRAL)
    {"AT+BLECONN", at_ble_conn},
#endif // (BLE_CFG_ROLE & BLE_CFG_ROLE_CENTRAL)

    {"AT+BLECONNPARAM", at_ble_conn_param},
    {"AT+BLEDISCONN", at_ble_dis_conn},
    {"AT+BLEMTU", at_ble_mtu},
#if (BLE_APP_PHY_UPDATE_SUPPORT)
    {"AT+BLEPHY", at_ble_phy},
#endif //(BLE_APP_PHY_UPDATE_SUPPORT)
#if (BLE_APP_DATA_LEN_EXTEN_SUPPORT)
    {"AT+BLEDATALEN", at_ble_data_len},
#endif // (BLE_APP_DATA_LEN_EXTEN_SUPPORT)
#endif // (BLE_CFG_ROLE & (BLE_CFG_ROLE_PERIPHERAL | BLE_CFG_ROLE_CENTRAL))
#endif
    {"", NULL}
};
const uint32_t AT_CMD_TABLE_SZ = (sizeof(atcmd_table) / sizeof(atcmd_table[0]));

#ifdef CONFIG_ATCMD_SPI

#if 0
int at_spi_hw_is_idle(void)
{
    if (spi_manager.stat ==SPI_Slave_Idle)
        return 1;

    if (spi_manager.stat == SPI_Slave_AT_Recv) {
        if (spi_nss_status_get() == RESET) {
            printf("nss error detected\r\n");
            return 0;
        }
        if (at_dma_get_cur_received_num(128) != 0) {
            printf("error detected, nss=%d\r\n", spi_nss_status_get());
            return 0;
       } else {
            return (spi_nss_status_get() == SET);
       }
    }
    return 0;
}

#else
int at_spi_hw_is_idle(void)
{
    if (spi_manager.stat ==SPI_Slave_Idle)
        return 1;

    if (spi_nss_status_get() == RESET)
        return 0;

    if (spi_manager.stat == SPI_Slave_AT_Recv) {
        if (at_dma_get_cur_received_num(128) != 0) {
            printf("error detected, nss=%d\r\n", spi_nss_status_get());
            return 0;
        }

        return (spi_nss_status_get() == SET);
    }
    return 0;
}
#endif
/*!
    \brief      function used when SPI slave send to master using SPI DMA Tx
                function is mainly used when the input size is not fixed. An length
                header is added before the sent data.
    \param[in]  data send data
    \param[in]  size data size
    \retval     none
*/
void at_spi_send_with_handshake(char *data, int size)
{
    char *spi_send_data = NULL;
    int slen = size + SPI_SEND_LEN_FIELD;
    int ret = 0;

    if (size == 0 || data == NULL || size > SPI_SEND_LEN_MAX) {
        return;
    }

    /* 1. Prefix with spi header
        |----|----|----|----|----|
        |len3|len2|len1|len0|,   |
        |----|----|----|----|----|
    */
    spi_send_data = (char *)sys_malloc(slen);
    if (spi_send_data == NULL) {
        AT_TRACE("%s memory alloc fail\r\n", __func__);
        ret = -1;
        goto Exit;
    }

    snprintf(spi_send_data, SPI_SEND_LEN_FIELD + 1, "%04d,", size);
    sys_memcpy(spi_send_data + SPI_SEND_LEN_FIELD, data, size);

    // AT_TRACE_DATA("TX Data:", spi_send_data, slen);
    /* 2. Update spi slave stat */
    if (spi_manager.stat == SPI_Slave_AT_Recv)
        spi_manager.stat = SPI_Slave_AT_ACK;
    else if (spi_manager.stat == SPI_Slave_Data_Recv)
        spi_manager.stat = SPI_Slave_Data_ACK;
    else if (spi_manager.stat == SPI_Slave_File_Recv)
        spi_manager.stat = SPI_Slave_File_ACK;

    /* 3. Reconfig SPI using DMA TX */
    spi_manager.direction = SPI_Slave_TX_Dir;
    spi_dma_config(false, 0, true, (uint32_t)&spi_send_data[0], slen, 0);

    /* 4. Notify SPI Master that MISO is ready */
    spi_handshake_rising_trigger();

    /* 5. Waiting DMA TX done */
    ret = sys_sema_down(&at_hw_dma_sema, SPI_TRX_TIMEOUT);
    if (ret == OS_TIMEOUT) {
        AT_TRACE("TX timeout tx_cnt=%d, stat=%d, dir=%d\r\n", slen - dma_transfer_number_get(DMA_CH3),
                    spi_manager.stat, spi_manager.direction);
        AT_TRACE_DATA("TX Data:", spi_send_data, 50);
        ret = -2;
        goto Exit;
    }

    /* 6 update spi_manager state */
    AT_TRACE("send, s:%d, rx:%s, dir:%d\r\n", spi_manager.stat, at_hw_rx_buf, spi_manager.direction);
    if (spi_manager.stat == SPI_Slave_AT_ACK && (strncmp(at_hw_rx_buf, "AT+CIPSEND", 10) == 0)) {
        spi_manager.stat = SPI_Slave_Data_Recv;
        sys_memset(at_hw_rx_buf, 0, ATCMD_FIXED_LEN);
    } else if (spi_manager.stat == SPI_Slave_AT_ACK && (strncmp(at_hw_rx_buf, "AT+CIPSDFILE", 12) == 0)) {
        spi_manager.stat = SPI_Slave_File_Recv;
        sys_memset(at_hw_rx_buf, 0, ATCMD_FIXED_LEN);
    } else if (spi_manager.stat == SPI_Slave_File_ACK || spi_manager.stat == SPI_Slave_File_Recv ||
            spi_manager.stat == SPI_Slave_File_Done) {
        //TODO
    } else if (spi_manager.stat != SPI_Slave_AT_Recv) {
        AT_TRACE("send with handshake, stat=%d\r\n", spi_manager.stat);
    }

Exit:
    /* 7. free send data */
    if (spi_send_data)
        sys_mfree(spi_send_data);

    /* 8. back to atcmd recv mode */
    if (ret) {
        spi_manager.stat = SPI_Slave_Idle;
        at_spi_rcv_atcmd_config(0);
    }
}

void at_spi_rcv_atcmd_config(bool from_isr)
{
    uint32_t dma_channel;

    if (spi_manager.stat == SPI_Slave_Data_Recv ||
                spi_manager.stat == SPI_Slave_AT_Recv)
        AT_TRACE("Error, disorder seq %d, %s, stat %d\r\n",
                at_cmd_received, at_hw_rx_buf, spi_manager.stat);

    sys_memset(at_hw_rx_buf, 0, ATCMD_FIXED_LEN);
    at_hw_rx_buf_idx = 0;
    at_cmd_received = 0;

    /* 1. Reconfig SPI using DMA RX */
    spi_manager.direction = SPI_Slave_RX_Dir;
    spi_dma_config(true, (uint32_t)at_hw_rx_buf, false, 0, ATCMD_FIXED_LEN, from_isr);

    spi_manager.stat = SPI_Slave_AT_Recv;
}

/*!
    \brief      configure the SPI peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
void at_spi_init(void)
{
    spi_slave_init();

    at_hw_rx_buf[0] = '\0';
    at_hw_rx_buf_idx = 0;
    at_cmd_received = 0;

    spi_enable();

    at_spi_rcv_atcmd_config(0);

    spi_handshake_gpio_config();

    AT_TRACE("AT SPI Slave Initialized\r\n");
}

void at_spi_deinit(void)
{
    spi_disable();
    spi_deinit();
}

static void at_spi_reconfig_from_isr(void)
{

    if (spi_manager.rx_dma_done == 0 || spi_manager.tx_dma_done == 0)
        return;

    AT_TRACE("rx dma irq is raised,%d, %d, %s\r\n", spi_manager.stat, spi_manager.direction, at_hw_rx_buf);
    spi_manager.tx_dma_done = 0;
    spi_manager.rx_dma_done = 0;

    if (spi_manager.direction == SPI_Slave_RX_Dir) {
        if (SPI_Slave_Data_Recv == spi_manager.stat || SPI_Slave_File_Recv == spi_manager.stat) {
            sys_sema_up_from_isr(&at_hw_dma_sema);
            goto exit;
        }

        if (SPI_Slave_Data_Recv != spi_manager.stat) {
            if (spi_manager.stat == SPI_Slave_AT_Recv)
                spi_manager.stat = SPI_Slave_AT_ACK;
            at_cmd_received = 1;
            goto exit;
        }
    } else if (spi_manager.direction == SPI_Slave_TX_Dir) {
        sys_sema_up_from_isr(&at_hw_dma_sema);

        if ((spi_manager.stat == SPI_Slave_File_ACK) ||
                ((spi_manager.stat == SPI_Slave_AT_ACK) && (strncmp(at_hw_rx_buf, "AT+CIPSDFILE", 12) == 0))) {
            goto exit;
        } else if ((spi_manager.stat == SPI_Slave_AT_ACK) && (strncmp(at_hw_rx_buf, "AT+CIPSEND", 10) == 0)) {
            goto exit;
        }

        if ((spi_manager.stat == SPI_Slave_Data_ACK) || (spi_manager.stat == SPI_Slave_File_Done) ||
                (spi_manager.stat == SPI_Slave_AT_ACK)) {

            at_spi_rcv_atcmd_config(1);
        }
    }

exit:

    return;
}

#if 1

void at_spi_tx_dma_irq_hdl(uint32_t dma_channel)
{
    AT_TRACE("Tx irq raised\r\n");
    if (RESET != dma_interrupt_flag_get(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF);
        spi_manager.tx_dma_done = 1;
    }

    at_spi_reconfig_from_isr();
}

void at_spi_rx_dma_irq_hdl(uint32_t dma_channel)
{
    AT_TRACE("Rx irq raised\r\n");
    if (RESET != dma_interrupt_flag_get(SPI_RX_DMA_CH, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(SPI_RX_DMA_CH, DMA_INT_FLAG_FTF);
        spi_manager.rx_dma_done = 1;
    }

    at_spi_reconfig_from_isr();
}

#else
void at_spi_tx_dma_irq_hdl(uint32_t dma_channel)
{
    if (RESET != dma_interrupt_flag_get(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF);
    }
}

void at_spi_rx_dma_irq_hdl(uint32_t dma_channel)
{
    // Waiting TX done
    while (RESET == dma_interrupt_flag_get(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF));

    dma_interrupt_flag_clear(SPI_TX_DMA_CH, DMA_INT_FLAG_FTF);

    if (RESET != dma_interrupt_flag_get(dma_channel, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(dma_channel, DMA_INT_FLAG_FTF);

        printf("rx dma irq is raised,%d, %d, %s\r\n", spi_manager.stat, spi_manager.direction, at_hw_rx_buf);

        if (spi_manager.direction == SPI_Slave_RX_Dir) {
            if (SPI_Slave_Data_Recv == spi_manager.stat || SPI_Slave_File_Recv == spi_manager.stat) {
                sys_sema_up_from_isr(&at_hw_dma_sema);
                goto exit;
            }

            if (SPI_Slave_Data_Recv != spi_manager.stat) {
                at_cmd_received = 1;
                goto exit;
            }
        } else if (spi_manager.direction == SPI_Slave_TX_Dir) {
            sys_sema_up_from_isr(&at_hw_dma_sema);

            if (spi_manager.stat == SPI_Slave_File_ACK ||
                (spi_manager.stat == SPI_Slave_AT_ACK && (strncmp(at_hw_rx_buf, "AT+CIPSDFILE", 12) == 0))) {
                    goto exit;
            } else if (spi_manager.stat == SPI_Slave_AT_ACK && (strncmp(at_hw_rx_buf, "AT+CIPSEND", 10) == 0)) {
                goto exit;
            }


            if ((spi_manager.stat == SPI_Slave_Data_ACK) || (spi_manager.stat == SPI_Slave_File_Done) ||
                spi_manager.stat == SPI_Slave_AT_ACK) {

//            if ((spi_manager.stat == SPI_Slave_Data_ACK) || (spi_manager.stat == SPI_Slave_File_Done) ||
//                    (spi_manager.stat == SPI_Slave_AT_ACK && strncmp(at_hw_rx_buf, "AT+CIPSEND", 10))) {

                at_spi_rcv_atcmd_config(1);
            }

        }
    }
exit:
    return;
}
#endif

static void at_spi_dma_receive(uint32_t address, uint32_t num)
{
    int ret = OS_OK;

    /* 1. update spi manager dir */
    spi_manager.direction = SPI_Slave_RX_Dir;

    /* 2. Reconfig SPI using DMA Rx */
    spi_dma_config(true, address, false, 0, num, 0);

    /* 3. Notify Master using a pulse */
    spi_handshake_rising_trigger();

    /* 4. Waiting RX Done */
    ret = sys_sema_down(&at_hw_dma_sema, SPI_TRX_TIMEOUT);
    if (ret == OS_TIMEOUT) {
        AT_TRACE("receive timeout, rx_cnt=%d\r\n", num - dma_transfer_number_get(DMA_CH2));
        ret = -2;
        goto exit;
    }

exit:
    /* 5. back to atcmd recv if error occurs */
    if (ret) { //back to atcmd rcv
        AT_TRACE("back to atcmdrcv, stat=%d\r\n", spi_manager.stat);
        spi_manager.stat = SPI_Slave_Idle;
        at_spi_rcv_atcmd_config(0);
    }
}

static void at_spi_dma_receive_config(void)
{
    spi_tx_idle_wait();

    eclic_irq_enable(DMA_Channel2_IRQn, 8, 0);

    spi_interrupt_disable(SPI_INT_RBNE);
    rcu_periph_clock_enable(RCU_DMA);
    spi_dma_single_mode_config(DMA_PERIPH_TO_MEMORY);
}

static void at_spi_irq_receive_config(void)
{
    uint32_t dma_channel = DMA_CH2;

    eclic_irq_disable(DMA_Channel2_IRQn);
    spi_dma_disable(SPI_DMA_RECEIVE);
}

static void at_spi_dma_receive_start(uint32_t address, uint32_t num)
{
    uint32_t dma_channel;

    spi_dma_single_mode_config(DMA_PERIPH_TO_MEMORY);

    dma_channel = DMA_CH2;
    dma_memory_address_config(dma_channel, DMA_MEMORY_0, address);
    dma_transfer_number_config(dma_channel, num);
    dma_channel_enable(dma_channel);
    spi_dma_enable(SPI_DMA_RECEIVE);
}

static void at_spi_dma_receive_stop(void)
{
    uint32_t dma_channel;

    dma_channel = DMA_CH2;

    dma_interrupt_flag_clear(dma_channel, DMA_INT_FLAG_FTF);
    dma_interrupt_disable(dma_channel, DMA_INT_FTF);
    dma_channel_disable(dma_channel);
}

static void at_spi_dma_send_stop(void)
{
    uint32_t dma_channel;

    dma_channel = DMA_CH3;

    dma_interrupt_flag_clear(dma_channel, DMA_INT_FLAG_FTF);
    dma_interrupt_disable(dma_channel, DMA_INT_FTF);
    dma_channel_disable(dma_channel);
}

/*======================== SPI above ===================*/
#else
static void at_uart_rx_irq_hdl(uint32_t usart_periph)
{
    char ch;

    usart_interrupt_disable(usart_periph, USART_INT_RBNE);
    while (1) {
        // We should have chance to check overflow error
        // Otherwise it may cause dead loop handle rx interrupt
        if (RESET != usart_flag_get(usart_periph, USART_FLAG_ORERR)) {
            usart_flag_clear(usart_periph, USART_FLAG_ORERR);
        }

        if ((RESET != usart_flag_get(usart_periph, USART_FLAG_RBNE))) {
            ch = (char)usart_data_receive(usart_periph);
        } else {
            break;
        }

        if (ch == '\0') {
            break;
        }

        if (isprint(ch)) {
            at_hw_rx_buf[at_hw_rx_buf_idx++] = ch;
            if (at_hw_rx_buf_idx >= AT_HW_RX_BUF_SIZE) {
                at_hw_rx_buf_idx = 0;
            }
            //AT_TRACE("%c", ch);
        } else if (ch == '\r') { /* putty doesn't transmit '\n' */
            at_hw_rx_buf[at_hw_rx_buf_idx] = '\0';
            //AT_TRACE("\r\n");
            if (at_hw_rx_buf_idx > 0) {
                at_cmd_received = 1;
            }
            sys_wakelock_release(LOCK_ID_USART);
        } else if (ch == '\b') { /* non-destructive backspace */
            if (at_hw_rx_buf_idx > 0) {
                at_hw_rx_buf[--at_hw_rx_buf_idx] = '\0';
            }
        }
    }

    usart_interrupt_enable(usart_periph, USART_INT_RBNE);
}

static void at_uart_init(void)
{
    if (at_uart_conf.usart_periph == USART0) {
        eclic_irq_enable(USART0_IRQn, 0xb, 0);
    } else if (at_uart_conf.usart_periph == UART1) {
        eclic_irq_enable(UART1_IRQn, 0xb, 0);
    } else if (at_uart_conf.usart_periph == UART2) {
        eclic_irq_enable(UART2_IRQn, 0xb, 0);
    }
    uart_config(at_uart_conf.usart_periph, at_uart_conf.baudrate, false, false, false);
    uart_irq_callback_register(at_uart_conf.usart_periph, at_uart_rx_irq_hdl);
}

static void at_uart_deinit(void)
{
    uart_irq_callback_unregister(at_uart_conf.usart_periph);
    usart_interrupt_disable(at_uart_conf.usart_periph, USART_INT_RBNE);
    usart_deinit(at_uart_conf.usart_periph);
}

static void at_uart_send(char *data, int size)
{
    if (size == 0) {
        return;
    }

    sys_sema_down(&at_hw_tx_sema, 0);
    while (size > 0) {
        uart_putc_noint(at_uart_conf.usart_periph, *data++);
        size--;
    }
    sys_sema_up(&at_hw_tx_sema);
}

static bool at_uart_rx_is_ongoing(void)
{
    return (at_hw_rx_buf_idx > 0) ? true : false;
}

static void at_uart_dma_receive(uint32_t address, uint32_t num)
{
    uint32_t dma_channel;

    uart_tx_idle_wait(at_uart_conf.usart_periph);

    uart_config(at_uart_conf.usart_periph, at_uart_conf.baudrate, false, true, false);

    switch (at_uart_conf.usart_periph) {
    case USART0:
        eclic_irq_enable(DMA_Channel2_IRQn, 8, 0);
        dma_channel = DMA_CH2;
        break;
    case UART1:
        eclic_irq_enable(DMA_Channel0_IRQn, 8, 0);
        dma_channel = DMA_CH0;
        break;
    case UART2:
    default:
        eclic_irq_enable(DMA_Channel5_IRQn, 8, 0);
        dma_channel = DMA_CH5;
        break;
    }

    dma_memory_address_config(dma_channel, DMA_MEMORY_0, address);
    dma_transfer_number_config(dma_channel, num);
    dma_channel_enable(dma_channel);

    sys_sema_down(&at_hw_dma_sema, 0);

    usart_dma_receive_config(at_uart_conf.usart_periph, USART_RECEIVE_DMA_DISABLE);

    uart_config(at_uart_conf.usart_periph, at_uart_conf.baudrate, false, false, false);
}

static void at_uart_dma_receive_config(void)
{
    uart_tx_idle_wait(at_uart_conf.usart_periph);

    uart_config(at_uart_conf.usart_periph, at_uart_conf.baudrate, false, true, false);

    switch (at_uart_conf.usart_periph) {
    case USART0:
        eclic_irq_enable(DMA_Channel2_IRQn, 8, 0);
        break;
    case UART1:
        eclic_irq_enable(DMA_Channel0_IRQn, 8, 0);
        break;
    case UART2:
    default:
        eclic_irq_enable(DMA_Channel5_IRQn, 8, 0);
        break;
    }

}

static void at_uart_irq_receive_config(void)
{
    uint32_t dma_channel;

    switch (at_uart_conf.usart_periph) {
    case USART0:
        eclic_irq_disable(DMA_Channel2_IRQn);
        dma_channel = DMA_CH2;
        break;
    case UART1:
        eclic_irq_disable(DMA_Channel0_IRQn);
        dma_channel = DMA_CH0;
        break;
    case UART2:
    default:
        eclic_irq_disable(DMA_Channel5_IRQn);
        dma_channel = DMA_CH5;
        break;
    }

    usart_dma_receive_config(at_uart_conf.usart_periph, USART_RECEIVE_DMA_DISABLE);
    uart_config(at_uart_conf.usart_periph, at_uart_conf.baudrate, false, false, false);
}

static void at_uart_dma_receive_start(uint32_t address, uint32_t num)
{
    uint32_t dma_channel;

    uart_dma_single_mode_config(at_uart_conf.usart_periph, DMA_PERIPH_TO_MEMORY);

    switch (at_uart_conf.usart_periph) {
    case USART0:
        dma_channel = DMA_CH2;
        break;
    case UART1:
        dma_channel = DMA_CH0;
        break;
    case UART2:
    default:
        dma_channel = DMA_CH5;
        break;
    }

    dma_memory_address_config(dma_channel, DMA_MEMORY_0, address);
    dma_transfer_number_config(dma_channel, num);
    dma_channel_enable(dma_channel);
}

static void at_uart_dma_receive_stop(void)
{
    uint32_t dma_channel;

    switch (at_uart_conf.usart_periph) {
    case USART0:
        dma_channel = DMA_CH2;
        break;
    case UART1:
        dma_channel = DMA_CH0;
        break;
    case UART2:
    default:
        dma_channel = DMA_CH5;
        break;
    }

    dma_interrupt_flag_clear(dma_channel, DMA_INT_FLAG_FTF);
    dma_interrupt_disable(dma_channel, DMA_INT_FTF);
    dma_channel_disable(dma_channel);
}
#endif

void at_uart_rx_dma_irq_hdl(uint32_t dma_channel)
{
    if(RESET != dma_interrupt_flag_get(dma_channel, DMA_INT_FLAG_FTF)){
        dma_interrupt_flag_clear(dma_channel, DMA_INT_FLAG_FTF);
        sys_sema_up_from_isr(&at_hw_dma_sema);
    }
}

static void at_hw_init(void)
{
    memset(at_hw_rx_buf, 0, AT_HW_RX_BUF_SIZE);
    at_hw_rx_buf_idx = 0;

#ifdef CONFIG_ATCMD_SPI
    at_spi_init();
#else
    at_uart_init();
#endif
}

static void at_hw_deinit(void)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_deinit();
#else
    at_uart_deinit();
#endif

    memset(at_hw_rx_buf, 0, AT_HW_RX_BUF_SIZE);
    at_hw_rx_buf_idx = 0;
}

static void at_hw_send(char *data, int size)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_send_with_handshake(data, size);
#else
    at_uart_send(data, size);
#endif
}

static void at_hw_dma_receive(uint32_t address, uint32_t num)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_dma_receive(address, num);
#else
    at_uart_dma_receive(address, num);
#endif
}

uint32_t at_dma_get_cur_received_num(uint32_t size)
{
    uint32_t dma_channel;

#ifdef CONFIG_ATCMD_SPI
    dma_channel = DMA_CH2;
#else
    switch (at_uart_conf.usart_periph) {
    case USART0:
        dma_channel = DMA_CH2;
        break;
    case UART1:
        dma_channel = DMA_CH0;
        break;
    case UART2:
    default:
        dma_channel = DMA_CH5;
        break;
    }
#endif
    return (size - dma_transfer_number_get(dma_channel));
}

static void at_hw_dma_receive_config(void)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_dma_receive_config();
#else
    at_uart_dma_receive_config();
#endif
}

static void at_hw_dma_receive_start(uint32_t address, uint32_t num)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_dma_receive_start(address, num);
#else
    at_uart_dma_receive_start(address, num);
#endif
}

static void at_hw_dma_receive_stop(void)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_dma_receive_stop();
#else
    at_uart_dma_receive_stop();
#endif
}

static void at_hw_irq_receive_config(void)
{
#ifdef CONFIG_ATCMD_SPI
    at_spi_irq_receive_config();
#else
    at_uart_irq_receive_config();
#endif
}

static void at_hw_fill_rx_buf(char *p, uint8_t len)
{
    sys_memcpy(at_hw_rx_buf, p, len);
    at_cmd_received = 1;
#ifdef CONFIG_ATCMD_SPI
    sys_wakelock_release(LOCK_ID_SPI);
#else
    sys_wakelock_release(LOCK_ID_USART);
#endif
}

static char *get_next_param(char **params)
{
    char *ptr = *params, *next;
    char sep = AT_SEPARATE;

    if (!ptr)
        return NULL;

    if (ptr[0] == '\'') {
        sep = ptr[0];
        ptr++;
    }

    next = strchr(ptr, sep);
    if (next) {
        *next++ = '\0';
        while (*next == AT_SPACE || *next == AT_SEPARATE)
            next++;
        if (*next == '\0')
            next = NULL;
    }
    *params = next;
    return ptr;
}

/*!
    \brief      parse AT command
    \param[in]  atcmd: the pointer of AT command
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static int atcmd_parse(char *atcmd, char **argv)
{
    int32_t i;
    int32_t argc = 0;
    char *param;
    char *buf;
    uint32_t size;

    if (atcmd == NULL) {
        return -1;
    }

    buf = atcmd;
    size = strlen(atcmd);

    /* Get command */
    argv[argc] = buf;
    for (i = 0; i < size; i++) {
        if (*buf == AT_EQU) {
            break;
        } else if ((*buf == AT_CR) || (*buf == AT_LF)) {
            goto Exit;
        } else {
            buf++;
        }
    }
    if (i == size)
        goto Exit;

    /* Get parameters */
    *buf++ = '\0';
    argc++;

    param = get_next_param(&buf);
    while (param && (argc < AT_MAX_ARGC)) {
        argv[argc] = param;
        argc++;
        param = get_next_param(&buf);
    }

    for (i = 0; i < argc; i++) {
        AT_TRACE("argv[%d] = %s\r\n", i, argv[i]);
    }
    return argc;

Exit:
    *buf = '\0';
    argc++;
    return argc;
}

static void atcmd_task(void *param)
{
    int argc = 0, i = 0;
    char *argv[AT_MAX_ARGC];

#ifdef CFG_WLAN_SUPPORT
    wifi_wait_ready();
#endif

#ifdef CFG_BLE_SUPPORT
    ble_wait_ready();
#endif

#ifndef CONFIG_ATCMD_SPI
    AT_RSP_DIRECT("READY\r\n", 7);
#endif

    for (;;) {
        while (at_cmd_received == 0) {
            sys_ms_sleep(2);
        }

        if (at_task_exit == 1)
            break;

#ifdef CONFIG_ATCMD_SPI
        if (spi_manager.stat != SPI_Slave_AT_ACK) {
            at_cmd_received = 0;
            AT_TRACE("Unexpected, %s, spi_manager->stat=%d\r\n", at_hw_rx_buf, spi_manager.stat);
            at_spi_rcv_atcmd_config(0);
            goto cont;
       }
#endif

        if (at_hw_rx_buf[0] == '\0') {
            at_cmd_received = 0;
            goto cont;
        } else {
            argc = atcmd_parse(at_hw_rx_buf, argv);
            if (argc == 0)
                goto cont;
        }
        for (i = 0; i < AT_CMD_TABLE_SZ - 1; i++) {
            if ((strcmp(argv[0], atcmd_table[i].name) == 0) ||
                (strncmp(argv[0], atcmd_table[i].name, strlen(atcmd_table[i].name)) == 0
                && argv[0][strlen(argv[0]) - 1] == AT_QUESTION
                && (strlen(argv[0]) == strlen(atcmd_table[i].name) + 1))) {
                if (atcmd_table[i].exec) {
                    atcmd_table[i].exec(argc, argv);
                }
                break;
            }
        }

        if (i == AT_CMD_TABLE_SZ - 1) {//not found
            AT_TRACE("Invalid atcmd, %s\r\n", at_hw_rx_buf);
            AT_RSP_DIRECT("ERROR\r\n", 7);
        }
cont:
        AT_TRACE("# ");
#ifndef CONFIG_ATCMD_SPI
        at_hw_rx_buf[0] = '\0';
        at_hw_rx_buf_idx = 0;
        at_cmd_received = 0;
#endif
    }

    sys_task_delete(NULL);
}

int atcmd_init(void)
{
    int ret = 0;

#ifndef CONFIG_ATCMD_SPI
    if (LOG_UART == AT_UART) {
        AT_TRACE("AT_UART and LOG_UART select the same uart. Please seperate them.\r\n");
        ret = -1;
        goto Exit;
    }
#endif
    at_cmd_received = 0;

    at_hw_init();
    cip_info_init();

    if (sys_sema_init(&at_hw_dma_sema, 0)) {
        ret = -2;
        goto Exit;
    }

    if (sys_sema_init(&at_hw_tx_sema, 1)) {
        ret = -3;
        goto Exit;
    }

    if (sys_sema_init(&at_ble_async_sema, 0)) {
        ret = -4;
        goto Exit;
    }

    if (sys_task_create_dynamic((const uint8_t *)"ATCMD",
            ATCMD_STACK_SIZE, ATCMD_PRIORITY, atcmd_task, NULL) == NULL) {
        ret = -5;
    }

Exit:
    if (ret) {
        AT_TRACE("%s: return %d\r\n", __func__, ret);

        if (at_hw_dma_sema) {
            sys_sema_free(&at_hw_dma_sema);
            at_hw_dma_sema = NULL;
        }

        if (at_hw_tx_sema) {
            sys_sema_free(&at_hw_tx_sema);
            at_hw_tx_sema = NULL;
        }

        if (at_ble_async_sema) {
            sys_sema_free(&at_ble_async_sema);
            at_ble_async_sema = NULL;
        }
    }
    return ret;
}

void atcmd_deinit(void)
{
    at_task_exit = 1;

    sys_ms_sleep(100);  //wait atcmd task finished and idle task released the task tcb

    at_hw_deinit();

    cip_info_reset();

    if (at_hw_dma_sema) {
        sys_sema_free(&at_hw_dma_sema);
        at_hw_dma_sema = NULL;
    }

    if (at_hw_tx_sema) {
        sys_sema_free(&at_hw_tx_sema);
        at_hw_tx_sema = NULL;
    }

    if (at_ble_async_sema) {
        sys_sema_free(&at_ble_async_sema);
        at_ble_async_sema = NULL;
    }
}
#endif

#if 0
static void atcmd_common_help_cb(void)
{
    AT_RSP("\t%s\n", atcmd_table[0].name);
    return;
}

static uint8_t atcmd_common_handle(void *data, void **cmd)
{
    const struct atcmd_entry *w_cmd = atcmd_table;
    char *data_tmp = (char *)data;

    if (cmd_mode_type_get() != CMD_MODE_TYPE_AT && strcmp(data_tmp, "AT"))
    {
        return CLI_UNKWN_CMD;
    }

    while (w_cmd->exec) {
        if (!strcmp(data_tmp, w_cmd->name) ||
            (!strncmp(data_tmp, w_cmd->name, strlen(w_cmd->name)) && data_tmp[strlen(w_cmd->name)] == AT_QUESTION))
        {
            *cmd = w_cmd->exec;
            break;
        }
        w_cmd++;
    }
    if (w_cmd->exec == NULL) {
        at_rsp_error();
        return CLI_UNKWN_CMD;
    }
    return CLI_SUCCESS;
}

/*!
    \brief      parse AT command
    \param[in]  atcmd: the pointer of AT command
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static int atcmd_parse(char *atcmd, char **argv)
{
    int32_t i;
    int32_t argc = 0;

    if (atcmd == NULL) {
        return -1;
    }
    char *buf = atcmd;
    uint32_t size = strlen(atcmd);

    /* Get command */
    argv[argc] = buf;
    for (i = 0; i < size; i++) {
        if (*buf == AT_EQU) {
            break;
        } else if ((*buf == AT_CR) || (*buf == AT_LF)) {
            goto Exit;
        } else {
            buf++;
        }
    }
    if (i == size)
        goto Exit;

    /* Get parameters */
    *buf++ = '\0';
    argc++;
    while((argc < AT_MAX_ARGC) && (*buf != '\0')) {
        argv[argc] = buf;
        argc++;
        buf++;

        while ((*buf != AT_SEPARATE) && (*buf != '\0')) {
            buf++;
        }
        if (*buf != '\0') {
            *buf = '\0';
            buf ++;
        }
        while (*buf == AT_SPACE) {
            *buf = '\0';
            buf ++;
        }
    }

    return argc;

Exit:
    *buf = '\0';
    argc++;
    return argc;
}

int atcmd_init(void)
{
    return cmd_module_reg(CMD_MODULE_ATCMD, NULL, atcmd_common_handle, atcmd_common_help_cb, atcmd_parse);
}
#endif
