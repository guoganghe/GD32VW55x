/*
 * iperf, Copyright (c) 2014, 2015, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */

/*!
    \file    iperf3_main.c
    \brief   Main function for iperf3.

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

#include <string.h>
#include "app_cfg.h"

#ifdef CONFIG_IPERF3_TEST
#include "wrapper_os.h"
#include "dbg_print.h"
#include "cmd_shell.h"
#include <lwip/sockets.h>

#include "iperf_queue.h"
#include "iperf_timer.h"
#include "iperf.h"
#include "iperf_api.h"

#undef iperf_err
#define iperf_err(a, fmt, arg...)       \
        do {                            \
            app_print("\r\n"fmt, ##arg);   \
        } while(0);

unsigned char iperf_terminate;
unsigned char iperf_task_num = 0;

os_task_t *iperf_task_tcb_ptr[IPERF_TASK_MAX] = {NULL};
void cmd_iperf3(int argc, char **argv);

extern void gnu_getopt_reset(void);

/**************************************************************************/
static int iperf_run(struct iperf_test *test)
{
    int consecutive_errors;

    switch (test->role) {
    case 's':
        if (test->daemon) {
            int rc = 0; //daemon(0, 0);
            if (rc < 0) {
                i_errno = 133; //IEDAEMON;
                iperf_err(test, "error - %s", iperf_strerror(i_errno));
                return (-1);
            }
        }
        consecutive_errors = 0;

        for (;;) {
            if (iperf_terminate) {
                break;
            }
            if (iperf_run_server(test) < 0) {
                iperf_err(test, "error - %s", iperf_strerror(i_errno));
                ++consecutive_errors;
                if (consecutive_errors >= 5) {
                    iperf_err(test, "too many errors, exiting");
                    return (-1);
                }
            } else {
                consecutive_errors = 0;
            }
            iperf_reset_test(test);
            if (iperf_get_test_one_off(test))
                break;
        }

        break;
    case 'c':
        if (iperf_run_client(test) < 0) {
            iperf_err(test, "error - %s", iperf_strerror(i_errno));
            return (-1);
        }
        break;
    default:
        usage();
        break;
    }

    return 0;
}

void iperf_test_task(void *param)
{
    struct iperf_test *test = (struct iperf_test *)param;
    unsigned int tcb_index = test->task_tcb_index;

    SYS_SR_ALLOC();

    if (iperf_run(test) < 0) {
        iperf_err(test, "error - %s", iperf_strerror(i_errno));
        // iperf_free_test(test);
        // return;
    }

    iperf_free_test(test);

    //app_print("\r\niperf3 task: used stack = %d, free stack = %d\r\n",
    //            (IPERF3_STACK_SIZE - sys_stack_free_get(NULL)), sys_stack_free_get(NULL));

    app_print("Iperf3 task stopped!\r\n");

    sys_enter_critical();
    iperf_task_tcb_ptr[tcb_index] = NULL;
    iperf_task_num--;
    sys_exit_critical();

    sys_task_delete(NULL);
}

/**************************************************************************/
int iperf3_main(int argc, char **argv)
{
    struct iperf_test *test;
    unsigned int i;
    unsigned int task_priority;
    void *handle;
    SYS_SR_ALLOC();

    iperf_terminate = 0;
    test = iperf_new_test();
    if (!test) {
        iperf_err(NULL, "create new test error - %s", iperf_strerror(i_errno));
        return (-1);
    }

    iperf_defaults(test);    /* sets defaults */

    if (iperf_parse_arguments(test, argc, argv) < 0) {
        iperf_err(test, "parameter error - %s", iperf_strerror(i_errno));
        //usage_long();
        usage();
        gnu_getopt_reset();
        iperf_free_test(test);
        return (-1);
    }

    gnu_getopt_reset();

    if (((test->role == 'c') || (test->role == 's')) && (iperf_task_num < IPERF_TASK_MAX)) {

        for ( i = 0; i < IPERF_TASK_MAX; i++) {
            if (iperf_task_tcb_ptr[i] == NULL) {
                test->task_tcb_index = i;
                break;
            }
        }

        if (i == IPERF_TASK_MAX) {
            app_print( "\r\n\rERROR: can not find available iperf3 task tcb.");
            iperf_free_test(test);
            return (-1);
        }

        task_priority = IPERF3_TASK_PRIO;

#if 0
        if (test->role == 'c') {
            // app_print( "\r\n\rtask priority, tos = %d", test->settings->tos);
            if (test->settings->tos == 0xe0) {
                task_priority = IPERF3_TASK_PRIO;
            } else if (test->settings->tos == 0xa0) {
                task_priority = IPERF3_TASK_PRIO + TASK_PRIO_LOWER(1);
            } else if (test->settings->tos == 0x20) {
                task_priority = IPERF3_TASK_PRIO + TASK_PRIO_LOWER(2);
            }
        }
#endif

        handle = sys_task_create_dynamic((const uint8_t *)"iperf3",
                IPERF3_STACK_SIZE, task_priority,
                (task_func_t)iperf_test_task, test);
        if (handle == NULL) {
            app_print( "\r\n\rERROR: create iperf3 task failed.");
            iperf_free_test(test);
            return (-1);
        } else {
            sys_enter_critical();
            iperf_task_tcb_ptr[i] = handle;
            iperf_task_num++;
            sys_exit_critical();
            return 0;
        }

    } else {
        iperf_free_test(test);
        return 0;
    }
}

void cmd_iperf3(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "-s") == 0) {
            app_print("\r\nIperf3: start iperf3 server!\r\n");
        } else if (strcmp(argv[1], "-c") == 0) {
            app_print("\r\nIperf3: start iperf3 client!\r\n");
        } else if (strcmp(argv[1], "-h") == 0) {
            goto Usage;
        } else if (strcmp(argv[1], "stop") == 0) {
            iperf_terminate = 1;
            return;
        } else {
            goto Exit;
        }
    } else {
        goto Exit;
    }

    iperf3_main(argc, argv);

    return;
Exit:
    app_print("\r\nIperf3: command format error!\r\n");
Usage:
    app_print("\rUsage:\r\n");
    app_print("    iperf3 <-s|-c hostip|stop|-h> [options]\r\n");
    app_print("\rServer or Client:\r\n");
    app_print("    -i #         seconds between periodic bandwidth reports\r\n");
    app_print("    -p #         server port to listen on/connect to\r\n");
    app_print("\rServer specific:\r\n");
    app_print("    -s           run in server mode\r\n");
    app_print("\rClient specific:\r\n");
    app_print("    -c <host>    run in client mode, connecting to <host>\r\n");
    app_print("    -u           use UDP rather than TCP\r\n");
    app_print("    -b #[KMG][/#] target bandwidth in bits/sec (0 for unlimited)\r\n");
    app_print("                 (default 1 Mbit/sec for UDP, unlimited for TCP)\r\n");
    app_print("                 (optional slash and packet count for burst mode)\r\n");
    app_print("    -t #         time in seconds to transmit for (default 10 secs)\r\n");
    app_print("    -l #[KMG]    length of buffer to read or write\r\n");
    app_print("    -S #         set the IP 'type of service'\r\n");
}

#endif /* CONFIG_IPERF3_TEST */
