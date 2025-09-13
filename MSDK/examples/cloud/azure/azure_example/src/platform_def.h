/*!
    \file    platform_def.h
    \brief   Platform definition for GD32VW55x SDK

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

#ifndef _PLATFORM_DEF_H
#define _PLATFORM_DEF_H

#define PLATFORM_FPGA_32103_V7          1
#define PLATFORM_FPGA_32103_ULTRA       2
#define PLATFORM_ASIC_32103             103

// platform type
#define CONFIG_PLATFORM                 PLATFORM_ASIC_32103

#ifndef CONFIG_PLATFORM
#error "CONFIG_PLATFORM must be defined!"
#elif CONFIG_PLATFORM >= PLATFORM_ASIC_32103
#define CONFIG_PLATFORM_ASIC
#else
#define CONFIG_PLATFORM_FPGA
#endif

// board type
#define PLATFORM_BOARD_32VW55X_START    0
#define PLATFORM_BOARD_32VW55X_EVAL     1
#define PLATFORM_BOARD_32VW55X_F527     2

#ifdef CONFIG_PLATFORM_ASIC
#define CONFIG_BOARD                    PLATFORM_BOARD_32VW55X_START
#endif

// RF type
#define RF_GDM32106                     0
#define RF_GDM32110                     1
#define RF_GDM32103                     2

#ifdef CONFIG_PLATFORM_ASIC
#define CONFIG_RF_TYPE                  RF_GDM32103
#else
#define CONFIG_RF_TYPE                  RF_GDM32110
#endif

// CRYSTAL type
#define CRYSTAL_26M                     0
#define CRYSTAL_40M                     1
#define PLATFORM_CRYSTAL                CRYSTAL_40M

// Wireless mode
#define CFG_WLAN_SUPPORT
//#define CFG_BLE_SUPPORT
#if defined(CFG_WLAN_SUPPORT) && defined(CFG_BLE_SUPPORT)
  #define CFG_COEX
#endif

// Flag indicating if NVDS FLASH feature is supported or not
#define NVDS_FLASH_SUPPORT              1

#endif /* _PLATFORM_DEF_H */
