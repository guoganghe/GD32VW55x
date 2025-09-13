/*!
    \file    app_dfu_srv.c
    \brief   dfu server Application Module entry point.

    \version 2024-07-2, V1.0.0, firmware for GD32VW55x
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
#include "app_dfu_srv.h"
#include "app_dfu_def.h"
#include "ble_ota_srv.h"
#include "wrapper_os.h"
#include "dbg_print.h"
#include "rom_export.h"
#include "raw_flash_api.h"
#include "config_gdm32.h"
#include "gd32vw55x.h"

#if FEAT_SUPPORT_BLE_OTA
typedef struct
{
    uint8_t  state;
    uint8_t  dfu_mode;
    uint8_t  working_bank;
    uint32_t new_img_addr;
    uint32_t total_bank_size;
    uint32_t ota_img_size;
    uint32_t cur_offset;
    uint16_t temp_buf_used_size;
    uint8_t  *p_tem_buf;
    uint32_t erase_start_addr;
    mbedtls_sha256_context sha256_context;
} dfu_srv_env_t;

typedef enum
{
    DFU_STATE_SRV_IDLE               ,
    DFU_STATE_SRV_MODE_GET           ,
    DFU_STATE_SRV_IMAGE_SIZE_GET     ,
    DFU_STATE_SRV_DFU_STARTED        ,
    DFU_STATE_SRV_DFU_FINISHED       ,
    DFU_STATE_SRV_VERIFICATION_PASS  ,
    DFU_STATE_SRV_WAIT_LAST_CMD      ,
    DFU_STATE_SRV_DISCONNECTING      ,
} ble_dfu_srv_state_t;

static dfu_srv_env_t dfu_srv_env;
os_timer_t dfu_srv_timer;

const dfu_cmd_cb_t dfu_srv_cmd_cb[DFU_OPCODE_MAX] = {
    [DFU_OPCODE_MODE]           = {2, 10000              },
    [DFU_OPCODE_IMAGE_SIZE]     = {5, DFU_TIMEOUT_DEFAULT},
    [DFU_OPCODE_START_DFU]      = {1, 60000              },   //for image transmit
#if FEAT_VALIDATE_FW_SUPPORT
    [DFU_OPCODE_VERIFICATION]   = {33, DFU_TIMEOUT_DEFAULT},
#else
    [DFU_OPCODE_VERIFICATION]   = {1, DFU_TIMEOUT_DEFAULT},
#endif
    [DFU_OPCODE_REBOOT]         = {1, DFU_TIMEOUT_DEFAULT},
    [DFU_OPCODE_RESET ]         = {2, DFU_TIMEOUT_DEFAULT},
};

static void app_dfu_srv_state_set(ble_dfu_srv_state_t state)
{
    dfu_srv_env.state = state;
}

static bool app_dfu_srv_state_check(ble_dfu_srv_state_t state)
{
    if (dfu_srv_env.state == state)
        return true;
    else
        return false;
}

void app_dfu_srv_reset(void)
{
    sys_memset(&dfu_srv_env, 0, sizeof(dfu_srv_env_t));
    sys_timer_stop(&dfu_srv_timer, false);
}

static void app_dfu_srv_data_cb(uint16_t data_len, uint8_t *p_data)
{
    int32_t ret = 0;

    if(!app_dfu_srv_state_check(DFU_STATE_SRV_DFU_STARTED)) {
        dbg_print(NOTICE, "dfu procdure has not been started\r\n");
        app_dfu_srv_reset();
        return;
    }

    sys_memcpy(dfu_srv_env.p_tem_buf + dfu_srv_env.temp_buf_used_size, p_data, data_len);
    dfu_srv_env.temp_buf_used_size += data_len;

    if (dfu_srv_env.temp_buf_used_size == FLASH_WRITE_SIZE || (dfu_srv_env.temp_buf_used_size + dfu_srv_env.cur_offset) == dfu_srv_env.ota_img_size) {
#if FEAT_VALIDATE_FW_SUPPORT
        mbedtls_sha256_update(&dfu_srv_env.sha256_context, dfu_srv_env.p_tem_buf, dfu_srv_env.temp_buf_used_size);
#endif

        ret = raw_flash_write((dfu_srv_env.new_img_addr + dfu_srv_env.cur_offset), dfu_srv_env.p_tem_buf, dfu_srv_env.temp_buf_used_size);
        if (ret < 0)
            dbg_print(NOTICE, "flash write fail\r\n");
        dfu_srv_env.cur_offset += dfu_srv_env.temp_buf_used_size;
        dfu_srv_env.temp_buf_used_size = 0;
        raw_flash_erase(dfu_srv_env.erase_start_addr, FLASH_WRITE_SIZE);
        dfu_srv_env.erase_start_addr += FLASH_WRITE_SIZE;
    }

    if (dfu_srv_env.cur_offset == dfu_srv_env.ota_img_size) {
        dbg_print(NOTICE, "image transmit finished\r\n");
        app_dfu_srv_state_set(DFU_STATE_SRV_DFU_FINISHED);
    }

}

static void app_dfu_srv_control_cb(uint16_t data_len, uint8_t *p_data)
{
    uint8_t opcode = *p_data;
    int32_t ret = 0;
    uint8_t cmd[CMD_MAX_LEN];
    uint8_t error_code = 0;

    sys_timer_stop(&dfu_srv_timer, false);

    dbg_print(INFO,"app_dfu_srv_control_callback, opcode: %d\r\n", opcode);

    if (data_len != dfu_srv_cmd_cb[opcode].dfu_cmd_len) {
        error_code = DFU_ERROR_WRONG_LENGTH;
        goto error;
    }

    switch (opcode) {
    case DFU_OPCODE_MODE: {
        uint8_t mode = *(p_data + 1);

        if(!app_dfu_srv_state_check(DFU_STATE_SRV_IDLE)) {
            return;
        }

        if (mode == DFU_MODE_BLE) {
            dfu_srv_env.dfu_mode = DFU_MODE_BLE;
            rom_sys_status_get(SYS_RUNNING_IMG, LEN_SYS_RUNNING_IMG, &dfu_srv_env.working_bank);
            if (dfu_srv_env.working_bank) {
                dfu_srv_env.new_img_addr  = RE_IMG_0_OFFSET;
                dfu_srv_env.total_bank_size = RE_IMG_1_OFFSET - RE_IMG_0_OFFSET;
            }
            else {
                dfu_srv_env.new_img_addr  = RE_IMG_1_OFFSET;
                dfu_srv_env.total_bank_size = RE_IMG_1_END - RE_IMG_1_OFFSET;
            }
            dfu_srv_env.erase_start_addr = dfu_srv_env.new_img_addr;

#if FEAT_VALIDATE_FW_SUPPORT
            mbedtls_sha256_init(&dfu_srv_env.sha256_context);
            mbedtls_sha256_starts(&dfu_srv_env.sha256_context, 0);
#endif

            sys_timer_start_ext(&dfu_srv_timer, dfu_srv_cmd_cb[opcode].timeout, false);
            app_dfu_srv_state_set(DFU_STATE_SRV_MODE_GET);
        }
    } break;

    case DFU_OPCODE_IMAGE_SIZE:{
        uint32_t size = ble_read32(p_data + 1);

        if(!app_dfu_srv_state_check(DFU_STATE_SRV_MODE_GET)) {
            return;
        }

        dfu_srv_env.ota_img_size = size;
//        raw_flash_erase(dfu_srv_env.new_img_addr, dfu_srv_env.ota_img_size);    //left here for erasing time test

        raw_flash_erase(dfu_srv_env.erase_start_addr, FLASH_WRITE_SIZE);
        dfu_srv_env.erase_start_addr += FLASH_WRITE_SIZE;

        sys_timer_start_ext(&dfu_srv_timer, dfu_srv_cmd_cb[opcode].timeout, false);
        app_dfu_srv_state_set(DFU_STATE_SRV_IMAGE_SIZE_GET);
    } break;

    case DFU_OPCODE_START_DFU:{

        if(!app_dfu_srv_state_check(DFU_STATE_SRV_IMAGE_SIZE_GET)) {
            return;
        }
        dfu_srv_env.p_tem_buf = sys_malloc(FLASH_WRITE_SIZE);

        if (dfu_srv_env.p_tem_buf == NULL) {
            error_code = DFU_ERROR_MEMORY_CAPA_EXCEED;
            goto error;
        }

        sys_timer_start_ext(&dfu_srv_timer, dfu_srv_cmd_cb[opcode].timeout, false);
        app_dfu_srv_state_set(DFU_STATE_SRV_DFU_STARTED);
    } break;

    case DFU_OPCODE_VERIFICATION:{
        uint8_t sha256_result[SHA256_RESULT_SIZE];

        if(!app_dfu_srv_state_check(DFU_STATE_SRV_DFU_FINISHED)) {
            return;
        }

#if FEAT_VALIDATE_FW_SUPPORT
        mbedtls_sha256_finish(&dfu_srv_env.sha256_context, sha256_result);
        mbedtls_sha256_free(&dfu_srv_env.sha256_context);
        sys_mfree(dfu_srv_env.p_tem_buf);

        if (sys_memcmp(sha256_result, p_data + 1, SHA256_RESULT_SIZE)) {
            error_code = DFU_ERROR_HASH_ERROR;
            goto error;
        }
#endif

        sys_timer_start_ext(&dfu_srv_timer, dfu_srv_cmd_cb[opcode].timeout, false);
        app_dfu_srv_state_set(DFU_STATE_SRV_VERIFICATION_PASS);
    } break;

    case DFU_OPCODE_REBOOT:{
        if(!app_dfu_srv_state_check(DFU_STATE_SRV_VERIFICATION_PASS)) {
            return;
        }

        sys_timer_start_ext(&dfu_srv_timer, dfu_srv_cmd_cb[opcode].timeout, false);
        app_dfu_srv_state_set(DFU_STATE_SRV_WAIT_LAST_CMD);
    } break;

    case DFU_OPCODE_RESET:
        dbg_print(NOTICE, "peer ota procedure reset, error code : %d\r\n", *(p_data + 1));
        app_dfu_srv_reset();
        return;

    default:
        break;
    }

    cmd[0] = opcode;
    cmd[1] = DFU_ERROR_NO_ERROR;
    ble_ota_srv_tx(0, cmd, 2);

    return;

error:
    cmd[0] = opcode;
    cmd[1] = error_code;
    ble_ota_srv_tx(0, cmd, 2);
    dbg_print(NOTICE, "local dfu error, opcode: %d, error code : %d\r\n", opcode, error_code);
    app_dfu_srv_reset();
}

void app_dfu_srv_disconn_cb(uint8_t conn_idx)
{
    int32_t res = 0;

    if (!app_dfu_srv_state_check(DFU_STATE_SRV_DISCONNECTING)) {
        app_dfu_srv_reset();
        return;
    }
    res = rom_sys_set_img_flag(dfu_srv_env.working_bank, (IMG_FLAG_IA_MASK | IMG_FLAG_NEWER_MASK), (IMG_FLAG_IA_OK | IMG_FLAG_OLDER));
    res |= rom_sys_set_img_flag(!dfu_srv_env.working_bank, (IMG_FLAG_IA_MASK | IMG_FLAG_VERIFY_MASK | IMG_FLAG_NEWER_MASK), IMG_FLAG_NEWER);
    if (res != 0) {
        dbg_print(NOTICE, "image switch fail\r\n");
        app_dfu_srv_reset();
        return;
    }
    dbg_print(NOTICE,"dfu_srv_success\r\n");
    SysTimer_SoftwareReset();
}

void app_dfu_srv_ind_cb(uint8_t conn_idx)
{
    ble_status_t ret = 0;

    if (app_dfu_srv_state_check(DFU_STATE_SRV_WAIT_LAST_CMD)) {
        ret = ble_conn_disconnect(0, BLE_ERROR_HL_TO_HCI(BLE_LL_ERR_REMOTE_USER_TERM_CON));
        if (ret != BLE_ERR_NO_ERROR) {
            dbg_print(NOTICE,"disconnect connection fail status 0x%x\r\n", ret);
            return;
        }
        app_dfu_srv_state_set(DFU_STATE_SRV_DISCONNECTING);
    }
}

static void app_dfu_srv_ota_timer_timeout_cb( void *ptmr, void *p_arg )
{
    uint8_t cmd[2] = {0};

    dbg_print(NOTICE,"app_dfu_srv_ota_timer_timeout_cb, state: %d\r\n", dfu_srv_env.state);

    cmd[0] = DFU_OPCODE_RESET;
    cmd[1] = DFU_ERROR_TIMEOUT;
    ble_ota_srv_tx(0, cmd, 2);

    dfu_srv_env.state = DFU_STATE_SRV_IDLE;
}

void app_dfu_srv_init(void)
{
    ble_ota_srv_callbacks_t ota_callbacks = {
        .ota_data_callback     = app_dfu_srv_data_cb,
        .ota_control_callback  = app_dfu_srv_control_cb,
        .ota_disconn_callback  = app_dfu_srv_disconn_cb,
        .ind_send_callback     = app_dfu_srv_ind_cb,
    };

    ble_ota_srv_init(&ota_callbacks);
    sys_timer_init(&(dfu_srv_timer), (const uint8_t *)("dfu_srv_timer"),
        DFU_TIMEOUT_DEFAULT, 0, app_dfu_srv_ota_timer_timeout_cb, NULL);
    app_dfu_srv_reset();
}

void app_dfu_srv_deinit(void)
{
    app_dfu_srv_reset();
    ble_ota_srv_deinit();
}
#endif
