/*!
    \file    main.c
    \brief   Main loop of GD32VW55x SDK.

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

#include <stdint.h>
#include "wifi_export.h"
#include "gd32vw55x_platform.h"
#include "uart.h"
#include "ble_init.h"
#include "gd32vw55x.h"
#include "wrapper_os.h"
#include "cmd_shell.h"
#include "util.h"
#include "wlan_config.h"
#include "wifi_init.h"

#define DEMO_QUEUE_SIZE         100

static uint32_t thread_0_counter = 0;
static uint32_t thread_1_counter = 0;
static uint32_t thread_2_counter = 0;
static uint32_t thread_3_counter = 0;
static uint32_t thread_4_counter = 0;
static uint32_t thread_5_counter = 0;
static uint32_t thread_6_counter = 0;
static uint32_t thread_7_counter = 0;
static uint32_t thread_8_counter = 0;

static uint32_t thread_1_messages_sent = 0;
static uint32_t thread_2_messages_received = 0;

static task_wrapper_t *thread_0_wrapper = NULL;
static task_wrapper_t *thread_1_wrapper = NULL;
static task_wrapper_t *thread_2_wrapper = NULL;
static task_wrapper_t *thread_3_wrapper = NULL;
static task_wrapper_t *thread_4_wrapper = NULL;
static task_wrapper_t *thread_5_wrapper = NULL;
static task_wrapper_t *thread_6_wrapper = NULL;
static task_wrapper_t *thread_7_wrapper = NULL;
static task_wrapper_t *thread_8_wrapper = NULL;


static TX_EVENT_FLAGS_GROUP event_flags_0;

static os_queue_t queue_0 = NULL;

static os_sema_t semaphore_0 = NULL;

static os_mutex_t mutex_0 = NULL;

void platform_reset(uint32_t error)
{
}

/**
 ****************************************************************************************
 * @brief Main entry point.
 *
 * This function is called right after the booting process has completed.
 ****************************************************************************************
 */
int main(void)
{
    printf("start main \r\n");

    platform_init();

    sys_os_init();

    sys_os_start();

    for ( ; ; );
}

static void thread_0_entry(void *arg)
{
    UINT    status;
    app_print("%s start \r\n", thread_0_wrapper->tx_thread.tx_thread_name);

    /* This thread simply sits in while-forever-sleep loop.  */
    while(1)
    {
        /* Increment the thread counter.  */
        thread_0_counter++;

        sys_ms_sleep(10);

        /* Set event flag 0 to wakeup thread 5.  */
        status =  tx_event_flags_set(&event_flags_0, 0x1, TX_OR);

        /* Check status.  */
        if (status != TX_SUCCESS)
            break;

        //app_print("thread_0_entry %d \r\n", thread_0_counter);
    }
}

static void thread_1_entry(void *arg)
{

    int32_t status;

    app_print("%s start \r\n", thread_1_wrapper->tx_thread.tx_thread_name);

    /* This thread simply sends messages to a queue shared by thread 2.  */
    while(1)
    {

        /* Increment the thread counter.  */
        thread_1_counter++;

        /* Send message to queue 0.  */
        status = sys_queue_write(&queue_0, &thread_1_messages_sent, -1, false);
        /* Check completion status.  */
        if (status != OS_OK) {
            app_print("thread_1_entry send message to queue fail %d \r\n", thread_1_messages_sent);
            break;
        }

        //app_print("s %d \r\n", thread_1_messages_sent);

        /* Increment the message sent.  */
        thread_1_messages_sent++;
    }
}

static void thread_2_entry(void *arg)
{

    ULONG   received_message;
    int    status;

    app_print("%s start \r\n", thread_2_wrapper->tx_thread.tx_thread_name);
    /* This thread retrieves messages placed on the queue by thread 1.  */
    while(1)
    {

        /* Increment the thread counter.  */
        thread_2_counter++;

        /* Retrieve a message from the queue.  */
        status = sys_queue_read(&queue_0, &received_message, -1, false);

        /* Check completion status and make sure the message is what we
           expected.  */
        if ((status != OS_OK) || (received_message != thread_2_messages_received)) {
            app_print("thread_2_entry receive message from queue fail %d : %d \r\n", thread_2_messages_received, received_message);
            break;
        }

        //app_print("r %d\r\n", thread_2_messages_received);
        /* Otherwise, all is okay.  Increment the received message count.  */
        thread_2_messages_received++;
    }
}

static void thread_3_and_4_entry(void *arg)
{
    int32_t    status;
    task_wrapper_t * thread_input = *(task_wrapper_t **)(arg);


    /* This function is executed from thread 3 and thread 4.  As the loop
       below shows, these function compete for ownership of semaphore_0.  */
    while(1)
    {
        /* Increment the thread counter.  */
        if (thread_input == thread_3_wrapper)
            thread_3_counter++;
        else if (thread_input == thread_4_wrapper)
            thread_4_counter++;
        else {
            app_print("thread_3_and_4_entry args wrong \r\n");
            break;
        }

        /* Get the semaphore with suspension.  */
        status =  sys_sema_down(&semaphore_0, 0);

        /* Check status.  */
        if (status != OS_OK) {
            app_print("thread_3_and_4_entry sema down fail \r\n");
            break;
        }

        /* Sleep for 20 ticks to hold the semaphore.  */
        sys_ms_sleep(20);

        /* Release the semaphore.  */
        sys_sema_up(&semaphore_0);
    }
}

static void thread_5_entry(void *arg)
{
    UINT    status;
    uint32_t   actual_flags;

    app_print("%s start \r\n", thread_5_wrapper->tx_thread.tx_thread_name);

    /* This thread simply waits for an event in a forever loop.  */
    while(1)
    {

        /* Increment the thread counter.  */
        thread_5_counter++;

        /* Wait for event flag 0.  */
        status =  tx_event_flags_get(&event_flags_0, 0x1, TX_OR_CLEAR,
                                                &actual_flags, TX_WAIT_FOREVER);

        /* Check status.  */
        if ((status != TX_SUCCESS) || (actual_flags != 0x1)) {
            app_print("thread_5_entry event flags wrong \r\n");
            break;
        }
    }
}

static void thread_6_and_7_entry(void *arg)
{
    int32_t    status;
    task_wrapper_t * thread_input = *(task_wrapper_t **)(arg);

    /* This function is executed from thread 6 and thread 7.  As the loop
       below shows, these function compete for ownership of mutex_0.  */
    while(1)
    {

        /* Increment the thread counter.  */
        if (thread_input == thread_6_wrapper)
            thread_6_counter++;
        else if (thread_input == thread_7_wrapper)
            thread_7_counter++;
        else {
            app_print("thread_6_and_7_entry args wrong \r\n");
            break;
        }


        /* Get the mutex with suspension.  */
        status = sys_mutex_get(&mutex_0);

        /* Check status.  */
        if (status != OS_OK) {
            break;
        }

        /* Get the mutex again with suspension.  This shows
           that an owning thread may retrieve the mutex it
           owns multiple times.  */
        status =  sys_mutex_get(&mutex_0);

        /* Check status.  */
        if (status != OS_OK) {
            break;
        }

        /* Sleep for 20 ticks to hold the mutex.  */
        sys_ms_sleep(20);

        /* Release the mutex.  */
        sys_mutex_put(&mutex_0);

        /* Release the mutex again.  This will actually
           release ownership since it was obtained twice.  */
        sys_mutex_put(&mutex_0);
    }
}

static void thread_8_entry(void *arg)
{
    UINT    status;
    app_print("%s start \r\n", thread_8_wrapper->tx_thread.tx_thread_name);

    /* This thread simply sits in while-forever-sleep loop.  */
    while(1)
    {
        /* Increment the thread counter.  */
        sys_enter_critical();
        thread_8_counter++;
        sys_exit_critical();

        //sys_us_delay(100000);

        sys_ms_sleep(100);

        //app_print("thread_8_entry %d \r\n", thread_8_counter);
    }
}

void tx_application_define(void *first_unused_memory)
{
    printf("tx_application_define \r\n");

#ifdef TX_ENABLE_STACK_CHECKING
    _tx_thread_application_stack_error_handler = threadx_stack_error_handler;
#endif
    thread_0_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 0", 512, OS_TASK_PRIORITY(31),
                                (task_func_t)thread_0_entry, NULL);
    if (thread_0_wrapper == NULL) {
        printf("Create thread 0 task failed\r\n");
    }

#if 1
    thread_1_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 1", 512, OS_TASK_PRIORITY(20),
                                (task_func_t)thread_1_entry, NULL);
    if (thread_1_wrapper == NULL) {
        printf("Create thread 1 task failed\r\n");
    }

    if (sys_queue_init(&queue_0, DEMO_QUEUE_SIZE, 4) == OS_ERROR) {
        printf("Create queue 0 failed\r\n");
    }

    thread_2_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 2", 512, OS_TASK_PRIORITY(20),
                                (task_func_t)thread_2_entry, NULL);
    if (thread_2_wrapper == NULL) {
        printf("Create thread 2 task failed\r\n");
    }

    thread_3_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 3", 512, OS_TASK_PRIORITY(24),
                                (task_func_t)thread_3_and_4_entry, &thread_3_wrapper);
    if (thread_3_wrapper == NULL) {
        printf("Create thread 3 task failed\r\n");
    }
    else {
        sys_task_change_timeslice((void * )thread_3_wrapper, 0);
    }

    thread_4_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 4", 512, OS_TASK_PRIORITY(24),
                                (task_func_t)thread_3_and_4_entry, &thread_4_wrapper);
    if (thread_4_wrapper == NULL) {
        printf("Create thread 4 task failed\r\n");
    }
    else {
        sys_task_change_timeslice((void * )thread_4_wrapper, 0);
    }


    if (sys_sema_init(&semaphore_0, 1) != OS_OK) {
        printf("Create semaphore_0 failed\r\n");
    }

    thread_5_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 5", 512, OS_TASK_PRIORITY(28),
                                (task_func_t)thread_5_entry, NULL);
    if (thread_5_wrapper == NULL) {
        printf("Create thread 5 task failed\r\n");
    }
    else {
        sys_task_change_timeslice((void * )thread_5_wrapper, 0);
    }

    /* Create the event flags group used by threads 1 and 5.  */
    tx_event_flags_create(&event_flags_0, "event flags 0");

    thread_6_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 6", 512, OS_TASK_PRIORITY(24),
                                (task_func_t)thread_6_and_7_entry, &thread_6_wrapper);
    if (thread_6_wrapper == NULL) {
        printf("Create thread 6 task failed\r\n");
    }
    else {
        sys_task_change_timeslice((void * )thread_6_wrapper, 0);
    }

    thread_7_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 7", 512, OS_TASK_PRIORITY(24),
                                (task_func_t)thread_6_and_7_entry, &thread_7_wrapper);
    if (thread_7_wrapper == NULL) {
        printf("Create thread 7 task failed\r\n");
    }
    else {
        sys_task_change_timeslice((void * )thread_7_wrapper, 0);
    }

    sys_mutex_init(&mutex_0);

    if (mutex_0 == NULL) {
        printf("Create mutex 0 failed\r\n");
    }
#endif
    thread_8_wrapper = (task_wrapper_t *)sys_task_create_dynamic((const uint8_t *)"thread 8", 512, OS_TASK_PRIORITY(24),
                                (task_func_t)thread_8_entry, NULL);
    if (thread_8_wrapper == NULL) {
        printf("Create thread 8 task failed\r\n");
    }


    SysTick_Config(SYSTICK_TICK_CONST);
    /* Set SWI interrupt level to lowest level/priority, SysTimerSW as Vector Interrupt */
    ECLIC_SetShvIRQ(CLIC_INT_SFT, ECLIC_VECTOR_INTERRUPT);
    ECLIC_SetLevelIRQ(CLIC_INT_SFT, 0);
    ECLIC_EnableIRQ(CLIC_INT_SFT);
}

