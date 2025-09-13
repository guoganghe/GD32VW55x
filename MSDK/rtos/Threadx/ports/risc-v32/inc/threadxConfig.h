/*!
    \file    threadxConfig.h
    \brief   definitions for the threadx config.

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
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   ThreadX compatibility Kit                                          */
/**                                                                       */
/**************************************************************************/


#ifndef THREADX_CONFIG_H
#define THREADX_CONFIG_H

/* #define configENABLE_FPU                         0 */
/* #define configENABLE_MPU                         0 */

/* #define configUSE_PREEMPTION                     1 */
/* #define configSUPPORT_STATIC_ALLOCATION          1 */
/* #define configSUPPORT_DYNAMIC_ALLOCATION         1 */
/* #define configUSE_IDLE_HOOK                      0 */
/* #define configUSE_TICK_HOOK                      0 */
//#define configCPU_CLOCK_HZ                       ( SystemCoreClock ) //crm_get_cpu_freq()
//#define configTICK_RATE_HZ                         (1000u)
//#define configMAX_PRIORITIES                       (32u)
#define configMINIMAL_STACK_SIZE                   (512u)
#define configTOTAL_HEAP_SIZE                      (1024u * 128u)
#define configMAX_TASK_NAME_LEN                    (16)
/* #define configUSE_TRACE_FACILITY                 0 */
#define configUSE_16_BIT_TICKS                      0
/* #define configUSE_MUTEXES                        1 */
/* #define configQUEUE_REGISTRY_SIZE                0 */
/* #define configUSE_RECURSIVE_MUTEXES              1 */
/* #define configUSE_COUNTING_SEMAPHORES            1 */
/* #define configUSE_PORT_OPTIMISED_TASK_SELECTION  0 */

/* #define configMESSAGE_BUFFER_LENGTH_TYPE         size_t */
#define configSTACK_DEPTH_TYPE                     uint32_t

/* #define configUSE_CO_ROUTINES                    0   */
/* #define configMAX_CO_ROUTINE_PRIORITIES          (2) */

/* Software timer definitions. */
/* #define configUSE_TIMERS                         1   */
/* #define configTIMER_TASK_PRIORITY                (2) */
/* #define configTIMER_QUEUE_LENGTH                 10  */
/* #define configTIMER_TASK_STACK_DEPTH             256 */

/* Set the following definitions to 1 to include the API function, or zero
   to exclude the API function. */
/* #define INCLUDE_vTaskPrioritySet             1 */
/* #define INCLUDE_uxTaskPriorityGet            1 */
#define INCLUDE_vTaskDelete                     1  /* Set to 0 to disable task deletion and the idle task. */
/* #define INCLUDE_vTaskCleanUpResources        0 */
/* #define INCLUDE_vTaskSuspend                 1 */
/* #define INCLUDE_vTaskDelayUntil              1 */
/* #define INCLUDE_vTaskDelay                   1 */
/* #define INCLUDE_xTaskGetSchedulerState       1 */
/* #define INCLUDE_xTimerPendFunctionCall       1 */
/* #define INCLUDE_xQueueGetMutexHolder         1 */
/* #define INCLUDE_uxTaskGetStackHighWaterMark  0 */
/* #define INCLUDE_eTaskGetState                1 */

/* max syscall priority, a larger interrupt priorities value indicates a higher priority, priority support 0-15 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    10

/* Define to a macro invoked to check for invalid arguments. */
#define configASSERT(x)
/* #define configASSERT(x) if ((x) == 0) {taskDISABLE_INTERRUPTS(); for(;;) {};} */

/* Set to 1 to support auto initialization, see documentation for details. */
#define TX_FREERTOS_AUTO_INIT 0


//#define SYSTICK_TICK_CONST          (configCPU_CLOCK_HZ / configTICK_RATE_HZ)

#endif /* #ifndef THREADX_CONFIG_H */
