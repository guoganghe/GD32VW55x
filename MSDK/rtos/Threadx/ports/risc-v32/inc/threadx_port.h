/*!
    \file    threadx_port.h
    \brief   definitions for the porting threadx.

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

/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   ThreadX compatibility Kit                                          */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  01-10-2024      cheng.cai               Initial Version 6.1           */
/*                                                                        */
/**************************************************************************/

#ifndef THREADX_PORT_H
#define THREADX_PORT_H
#include "tx_user.h"
#include "tx_api.h"

/* portNOP() is not required by this port. */
#define portNOP()                   __NOP()
#define portSTACK_TYPE              unsigned long

#define SYSTICK_TICK_CONST          (OS_CPU_CLOCK_HZ / OS_TICK_RATE_HZ)


#ifdef TX_ENABLE_STACK_CHECKING
void threadx_stack_error_handler(TX_THREAD *p_thread);
#endif

void vPortCriticalInit(void);

void vPortEnterCritical(void);

void vPortExitCritical(void);

uint32_t vPortInCritical(void);

#endif /* #ifndef THREADX_PORT_H */
