/*!
    \file    threadx_port.c
    \brief   Implementation of porting threadx.

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

#include "threadxConfig.h"
#include "threadx_port.h"
#include "tx_thread.h"
#include "gd32vw55x.h"

/* Masks off all bits but the ECLIC MTH bits in the MTH register. */
#define portMTH_MASK                ( 0xFFUL )
static uint32_t txCriticalNesting = 0;
static uint8_t txMaxSysCallMTH = 255;
extern void _tx_timer_interrupt(void);


void eclic_mtip_handler(void)
{
    SysTick_Reload(SYSTICK_TICK_CONST);

    _tx_timer_interrupt();
}

void clear_sw_irq(void)
{
    /* Clear Software IRQ, A MUST */
    SysTimer_ClearSWIRQ();
}


void trigger_sched_interrupt(void)
{
    /* Set a software interrupt(SWI) request to request a context switch. */
    SysTimer_SetSWIRQ();
    /* Barriers are normally not required but do ensure the code is completely
    within the specified behaviour for the architecture. */
    __RWMB();
}

#ifdef TX_ENABLE_STACK_CHECKING
void threadx_stack_error_handler(TX_THREAD *p_thread)
{
    if (p_thread != NULL) {
        printf("thread %s stack error \r\n", p_thread->tx_thread_name);
    }
}
#endif

void vPortCriticalInit(void)
{
    uint8_t nlbits = __ECLIC_GetCfgNlbits();
    uint8_t intctlbits = __ECLIC_INTCTLBITS;
    uint8_t lvlbits, lfabits;
    uint8_t maxsyscallmth = 0;
    uint8_t temp;
    uint8_t max_syscall_prio = configMAX_SYSCALL_INTERRUPT_PRIORITY;

    if (nlbits <= intctlbits) {
        lvlbits = nlbits;
    } else {
        lvlbits = intctlbits;
    }

    lfabits = 8 - lvlbits;

    temp = ((1 << lvlbits) - 1);
    if (max_syscall_prio > temp) {
        max_syscall_prio = temp;
    }

    txMaxSysCallMTH = (max_syscall_prio << lfabits) | ((1 << lfabits) - 1);
}


void vPortEnterCritical(void)
{
    ECLIC_SetMth(txMaxSysCallMTH);
    __RWMB();

    txCriticalNesting++;

    /* This is not the interrupt safe version of the enter critical function so
    assert() if it is being called from an interrupt context.  Only API
    functions that end in "FromISR" can be used in an interrupt.  Only assert if
    the critical nesting count is 1 to protect against recursive calls if the
    assert function also uses a critical section. */
    if (txCriticalNesting == 1) {
        configASSERT((__ECLIC_GetMth() & portMTH_MASK) == txMaxSysCallMTH);
    }
}
/*-----------------------------------------------------------*/

void vPortExitCritical(void)
{
    configASSERT(txCriticalNesting);
    txCriticalNesting--;

    if (txCriticalNesting == 0) {
        ECLIC_SetMth(0);
        __RWMB();
    }
}

uint32_t vPortInCritical(void)
{
    return txCriticalNesting;
}

void checkSavedSpInSystemReturn(unsigned long sp)
{
    TX_THREAD * p_thread = NULL;

    TX_THREAD_GET_CURRENT(p_thread);

    if (p_thread != NULL && (sp > (unsigned long)p_thread->tx_thread_stack_end || (sp < (unsigned long)p_thread->tx_thread_stack_start))) {
        printf("checkSavedSpInSystemReturn error");
        while(1);
    }
}

void checkSavedSpInMsft(unsigned long sp)
{
    TX_THREAD * p_thread = NULL;

    TX_THREAD_GET_CURRENT(p_thread);

    if (p_thread != NULL && (sp > (unsigned long)p_thread->tx_thread_stack_end || (sp < (unsigned long)p_thread->tx_thread_stack_start))) {
        printf("checkSavedSpInMsft error");
        while(1);
    }
}

void checkSavedSpInSave(unsigned long sp)
{
    TX_THREAD * p_thread = NULL;

    TX_THREAD_GET_CURRENT(p_thread);

    if (p_thread != NULL && (sp > (unsigned long)p_thread->tx_thread_stack_end || (sp < (unsigned long)p_thread->tx_thread_stack_start))) {
        printf("checkSavedSpInSave error 0x%x ", sp);
        while(1);
    }
}


void threadx_pre_scheduler_initialization(void)
{

#ifdef TX_ENABLE_STACK_CHECKING
    _tx_thread_application_stack_error_handler = threadx_stack_error_handler;
#endif

    vPortCriticalInit();
    SysTick_Config(SYSTICK_TICK_CONST);
    /* Set SWI interrupt level to lowest level/priority, SysTimerSW as Vector Interrupt */
    ECLIC_SetShvIRQ(CLIC_INT_SFT, ECLIC_VECTOR_INTERRUPT);
    ECLIC_SetLevelIRQ(CLIC_INT_SFT, 0);
    ECLIC_EnableIRQ(CLIC_INT_SFT);
}

void tx_application_define(void *first_unused_memory)
{
    return;
}

