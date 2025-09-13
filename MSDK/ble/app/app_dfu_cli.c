/*!
    \file    app_dfu_cli.c
    \brief   dfu Application Module entry point.

    \version 2024-07-31, V1.0.0, firmware for GD32VW55x
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

#include <string.h>
#include "app_dfu_cli.h"
#include "app_dfu_def.h"
#include "ble_ota_cli.h"
#include "wrapper_os.h"
#include "dbg_print.h"
#include "rom_export.h"
#include "raw_flash_api.h"
#include "config_gdm32.h"
#include "gd32vw55x.h"
#include "ble_conn.h"

#if FEAT_SUPPORT_BLE_OTA
/* Connection parameters of ota */
#define BLE_CONN_OTA_INTV             7               // 7.5ms
#define BLE_CONN_OTA_LATENCY          0
#define BLE_CONN_OTA_SUPV_TOUT        500             // 5000ms

typedef enum
{
    DFU_STATE_CLI_IDLE               ,
    DFU_STATE_CLI_MODE_SET           ,
    DFU_STATE_CLI_IMAGE_SIZE_GET     ,
    DFU_STATE_CLI_DFU_STARTED        ,
    DFU_STATE_CLI_VERIFICATION       ,
    DFU_STATE_CLI_REBOOT             ,
} ble_dfu_cli_state_t;

typedef struct
{
    uint8_t  state;
    uint32_t img_total_size;
    uint32_t cur_offset;
    mbedtls_sha256_context sha256_context;
} dfu_cli_env_t;

const dfu_cmd_cb_t dfu_cli_cmd_cb[DFU_OPCODE_MAX] = {
    [DFU_OPCODE_MODE]           = {2, 10000               },   //for falsh erase
    [DFU_OPCODE_IMAGE_SIZE]     = {5, DFU_TIMEOUT_DEFAULT },
    [DFU_OPCODE_START_DFU]      = {1, 60000               },   //for image transmit
#if FEAT_VALIDATE_FW_SUPPORT
    [DFU_OPCODE_VERIFICATION]   = {33, DFU_TIMEOUT_DEFAULT},
#else
    [DFU_OPCODE_VERIFICATION]   = {1, DFU_TIMEOUT_DEFAULT },
#endif
    [DFU_OPCODE_REBOOT]         = {1, DFU_TIMEOUT_DEFAULT },
    [DFU_OPCODE_RESET ]         = {2, DFU_TIMEOUT_DEFAULT },

};

os_timer_t dfu_cli_timer;
dfu_cli_env_t dfu_cli_env;

void app_dfu_cli_reset(void)
{
    sys_memset(&dfu_cli_env, 0, sizeof(dfu_cli_env_t));
    sys_timer_stop(&dfu_cli_timer, false);
}

static void app_dfu_cli_state_set(ble_dfu_cli_state_t state)
{
    dfu_cli_env.state = state;
}

static bool app_dfu_cli_state_check(ble_dfu_cli_state_t state)
{
    if (dfu_cli_env.state == state)
        return true;
    else
        return false;
}

static void app_dfu_cli_control_cb(uint16_t data_len, uint8_t *p_data)
{
    uint8_t opcode = *p_data;
    uint8_t error_code = 0;
    uint8_t result = *(p_data + 1);
    uint8_t cmd[CMD_MAX_LEN] = {0};
    uint8_t cmd_len = 0;

    sys_timer_stop(&dfu_cli_timer, false);

    if (opcode != DFU_OPCODE_RESET && result != DFU_ERROR_NO_ERROR) {
        goto rsp_error;
    }

    dbg_print(INFO,"app_dfu_cli_control_cb, opcode: %d\r\n", opcode);

    switch (opcode) {
    case DFU_OPCODE_MODE: {
        if(!app_dfu_cli_state_check(DFU_STATE_CLI_MODE_SET)) {
            return;
        }

        cmd[0] = DFU_OPCODE_IMAGE_SIZE;
        ble_write32(cmd + 1, dfu_cli_env.img_total_size);
        cmd_len = dfu_cli_cmd_cb[DFU_OPCODE_IMAGE_SIZE].dfu_cmd_len;
        sys_timer_start_ext(&dfu_cli_timer, dfu_cli_cmd_cb[opcode].timeout, false);
        app_dfu_cli_state_set(DFU_STATE_CLI_IMAGE_SIZE_GET);
    } break;

    case DFU_OPCODE_IMAGE_SIZE: {
        if(!app_dfu_cli_state_check(DFU_STATE_CLI_IMAGE_SIZE_GET)) {
            return;
        }

        cmd[0] = DFU_OPCODE_START_DFU;
        cmd_len = dfu_cli_cmd_cb[DFU_OPCODE_START_DFU].dfu_cmd_len;

        sys_timer_start_ext(&dfu_cli_timer, dfu_cli_cmd_cb[opcode].timeout, false);
        app_dfu_cli_state_set(DFU_STATE_CLI_DFU_STARTED);
    } break;

    case DFU_OPCODE_START_DFU: {
        uint8_t data[BLE_TRANSMIT_SIZE] = {0};

        if(!app_dfu_cli_state_check(DFU_STATE_CLI_DFU_STARTED)) {
            return;
        }

        raw_flash_read(RE_IMG_1_OFFSET + dfu_cli_env.cur_offset, data, BLE_TRANSMIT_SIZE);

#if FEAT_VALIDATE_FW_SUPPORT
        mbedtls_sha256_update(&dfu_cli_env.sha256_context, data, BLE_TRANSMIT_SIZE);
#endif
        ble_ota_cli_write_data(0, data, BLE_TRANSMIT_SIZE);
        dfu_cli_env.cur_offset += BLE_TRANSMIT_SIZE;

        sys_timer_start_ext(&dfu_cli_timer, dfu_cli_cmd_cb[opcode].timeout, false);
        app_dfu_cli_state_set(DFU_STATE_CLI_VERIFICATION);
    } break;

    case DFU_OPCODE_VERIFICATION:{
        if(!app_dfu_cli_state_check(DFU_STATE_CLI_VERIFICATION)) {
            return;
        }

        cmd[0] = DFU_OPCODE_REBOOT;
        cmd_len = dfu_cli_cmd_cb[DFU_OPCODE_REBOOT].dfu_cmd_len;

        sys_timer_start_ext(&dfu_cli_timer, dfu_cli_cmd_cb[opcode].timeout, false);
        app_dfu_cli_state_set(DFU_STATE_CLI_REBOOT);
    } break;

    case DFU_OPCODE_REBOOT:
        if(!app_dfu_cli_state_check(DFU_STATE_CLI_REBOOT)) {
            return;
        }
        app_dfu_cli_reset();
        sys_timer_start_ext(&dfu_cli_timer, dfu_cli_cmd_cb[opcode].timeout, false);
        dbg_print(NOTICE,"dfu_cli_success\r\n");
        break;

    case DFU_OPCODE_RESET:
        dbg_print(NOTICE, "peer ota procedure reset, error code : %d\r\n", *(p_data + 1));
        app_dfu_cli_reset();
        return;

    default:
        break;
    }

    if (cmd_len)
        ble_ota_cli_write_cmd(0, cmd, cmd_len);

    return;

rsp_error:
    dbg_print(NOTICE,"peer rsp error, opcode = %d, reulst = %d\r\n", opcode, result);
    app_dfu_cli_reset();
    return;

}

static void app_dfu_cli_send_verification_cmd(void)
{
    uint8_t cmd[CMD_MAX_LEN];
    uint8_t cmd_len = 0;

    cmd[0] = DFU_OPCODE_VERIFICATION;

#if FEAT_VALIDATE_FW_SUPPORT
    mbedtls_sha256_finish(&dfu_cli_env.sha256_context, cmd + 1);
    mbedtls_sha256_free(&dfu_cli_env.sha256_context);
#endif

    cmd_len = dfu_cli_cmd_cb[DFU_OPCODE_VERIFICATION].dfu_cmd_len;

    if (cmd_len)
        ble_ota_cli_write_cmd(0, cmd, cmd_len);
}

void app_dfu_cli_data_tx_cb(ble_status_t status)
{
    int32_t ret = 0;
    uint8_t data[BLE_TRANSMIT_SIZE] = {0};
    uint16_t len = ((dfu_cli_env.img_total_size - dfu_cli_env.cur_offset) > BLE_TRANSMIT_SIZE ) ? BLE_TRANSMIT_SIZE : (dfu_cli_env.img_total_size - dfu_cli_env.cur_offset);

    if(!app_dfu_cli_state_check(DFU_STATE_CLI_VERIFICATION)) {
        return;
    }

    if (len > 0) {
        ret = raw_flash_read(RE_IMG_1_OFFSET + dfu_cli_env.cur_offset, data, len);
        if (ret < 0) {
            dbg_print(NOTICE, "flash read fail\r\n");
            app_dfu_cli_reset();
            return;
        }
#if FEAT_VALIDATE_FW_SUPPORT
        mbedtls_sha256_update(&dfu_cli_env.sha256_context, data, len);
#endif
        ble_ota_cli_write_data(0, data, len);
        dfu_cli_env.cur_offset += len;

        if (dfu_cli_env.cur_offset == dfu_cli_env.img_total_size) {
            app_dfu_cli_send_verification_cmd();
            dbg_print(NOTICE,"dfu finished pls check\r\n");
        }
    }
}

void app_dfu_cli_disconn_cb(uint8_t conn_idx)
{
    app_dfu_cli_reset();
}

static void app_dfu_cli_ota_timer_timeout_cb( void *ptmr, void *p_arg )
{
    uint8_t cmd[2] = {0};

    dbg_print(NOTICE,"app_dfu_cli_ota_timer_timeout_cb, state: %d\r\n", dfu_cli_env.state);

    cmd[0] = DFU_OPCODE_RESET;
    cmd[1] = DFU_ERROR_TIMEOUT;
    ble_ota_cli_write_cmd(0, cmd, 2);

    dfu_cli_env.state = DFU_STATE_CLI_IDLE;
}

void app_ble_dfu_start(uint8_t conidx, uint32_t img_size)
{

    uint8_t cmd[CMD_MAX_LEN];
    ble_status_t status = BLE_ERR_NO_ERROR;


    if(!app_dfu_cli_state_check(DFU_STATE_CLI_IDLE)) {
        dbg_print(NOTICE, "dfu cli procdure has been started\r\n");
        return;
    }
    app_dfu_cli_reset();
    dfu_cli_env.img_total_size = img_size;


    cmd[0] = DFU_OPCODE_MODE;
    cmd[1] = DFU_MODE_BLE;
    if(ble_ota_cli_write_cmd(conidx, cmd, dfu_cli_cmd_cb[DFU_OPCODE_MODE].dfu_cmd_len))
        return;

    if(ble_conn_param_update_req(conidx, BLE_CONN_OTA_INTV, BLE_CONN_OTA_INTV, BLE_CONN_OTA_LATENCY, BLE_CONN_OTA_SUPV_TOUT, 0, 0))
        return;

    app_dfu_cli_state_set(DFU_STATE_CLI_MODE_SET);

#if FEAT_VALIDATE_FW_SUPPORT
    mbedtls_sha256_init(&dfu_cli_env.sha256_context);
    mbedtls_sha256_starts(&dfu_cli_env.sha256_context, 0);
#endif

    dbg_print(NOTICE,"app_ble_dfu_start\r\n");
}

void app_dfu_cli_init(void)
{
    ble_ota_cli_callbacks_t ota_callbacks = {
        .ota_cli_rx_callback = app_dfu_cli_control_cb,
        .ota_cli_tx_callback = app_dfu_cli_data_tx_cb,
        .ota_cli_disconn_callback  = app_dfu_cli_disconn_cb,
    };

    ble_ota_cli_init(&ota_callbacks);
    sys_timer_init(&(dfu_cli_timer), (const uint8_t *)("dfu_cli_timer"),
        DFU_TIMEOUT_DEFAULT, 0, app_dfu_cli_ota_timer_timeout_cb, NULL);
    app_dfu_cli_reset();
}

void app_dfu_cli_deinit(void)
{
    app_dfu_cli_reset();
    ble_ota_cli_deinit();
}
#endif
