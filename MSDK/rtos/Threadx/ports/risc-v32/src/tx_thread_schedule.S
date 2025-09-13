/***************************************************************************
 * Copyright (c) 2024 Microsoft Corporation
 * Copyright (c) 2023, GigaDevice Semiconductor Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * SPDX-License-Identifier: MIT
 **************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   Thread                                                              */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


/* #define TX_SOURCE_CODE  */


/* Include necessary system files.  */

/* #include "tx_api.h"
#include "tx_thread.h"
#include "tx_timer.h"*/

#include "riscv_encoding.h"

.section .text
/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _tx_thread_schedule                                RISC-V32/IAR     */
/*                                                           6.1          */
/*  AUTHOR                                                                */
/*                                                                        */
/*    William E. Lamie, Microsoft Corporation                             */
/*    Tom van Leeuwen, Technolution B.V.                                  */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function waits for a thread control block pointer to appear in */
/*    the _tx_thread_execute_ptr variable.  Once a thread pointer appears */
/*    in the variable, the corresponding thread is resumed.               */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    _tx_initialize_kernel_enter          ThreadX entry function         */
/*    _tx_thread_system_return             Return to system from thread   */
/*    _tx_thread_context_restore           Restore thread-s context       */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  09-30-2020     William E. Lamie         Initial Version 6.1           */
/*                                                                        */
/**************************************************************************/
# In _tx_thread_schedule function  _tx_thread_current_ptr must be NULL

/* VOID   _tx_thread_schedule(VOID)
{  */
    .global  _tx_thread_schedule
_tx_thread_schedule:

    /* Enable interrupts.  */
    csrsi   mstatus, 0x08                               # Enable interrupts

    /* Wait for a thread to execute.  */
    /* do
    {  */

    la      t0, _tx_thread_execute_ptr                  # Pickup address of execute ptr
_tx_thread_schedule_loop:
    lw      t1, 0(t0)                                   # Pickup next thread to execute
    beqz    t1, _tx_thread_schedule_loop                # If NULL, wait for thread to execute

    /* }
    while(_tx_thread_execute_ptr == TX_NULL);  */

    /* Yes! We have a thread to execute.  Lockout interrupts and
       transfer control to it.  */
    csrci   mstatus, 0x08                               # Lockout interrupts

    /* ####### check execute ptr after disable interrupt ##########*/
    la      t0, _tx_thread_execute_ptr                  # Pickup address of execute ptr
    lw      t1, 0(t0)                                   # Pickup next thread to execute
    beqz    t1, _tx_thread_schedule                     # If NULL, return back

    /* ####### port start ##########*/
    /* Determine if an interrupt frame or a synchronous task suspension frame
       is present.  */
    lw      t2, 8(t1)                                   # Pickup execute thread-s stack
    lw      t3, 0(t2)                                   # Pickup stack type
    bnez    t3, _tx_thread_trigger_interrupt            # If 1, trigger software interrupt
    /* ####### port end ##########*/

    /* Setup the current thread pointer.  */
    /* _tx_thread_current_ptr =  _tx_thread_execute_ptr;  */

    la      t0, _tx_thread_current_ptr                  # Pickup current thread pointer address
    sw      t1, 0(t0)                                   # Set current thread pointer

    /* Increment the run count for this thread.  */
    /* _tx_thread_current_ptr -> tx_thread_run_count++;  */

    lw      t2, 4(t1)                                   # Pickup run count
    lw      t3, 24(t1)                                  # Pickup time slice value
    addi    t2, t2, 1                                   # Increment run count
    sw      t2, 4(t1)                                   # Store new run count

    /* Setup time-slice, if present.  */
    /* _tx_timer_time_slice =  _tx_thread_current_ptr -> tx_thread_time_slice;  */

    la      t2, _tx_timer_time_slice                    # Pickup time-slice variable address

    /* Switch to the thread-s stack.  */
    /* SP =  _tx_thread_execute_ptr -> tx_thread_stack_ptr;  */

    lw      sp, 8(t1)                                   # Switch to thread-s stack
    sw      t3, 0(t2)                                   # Store new time-slice*/

#ifdef TX_ENABLE_EXECUTION_CHANGE_NOTIFY

    call    _tx_execution_thread_enter                  # Call the thread execution enter function
#endif

_tx_thread_synch_return:

#ifdef __riscv_flen
    flw     f8, 0x3C(sp)                                # Recover fs0
    flw     f9, 0x40(sp)                                # Recover fs1
    flw     f18,0x44(sp)                                # Recover fs2
    flw     f19,0x48(sp)                                # Recover fs3
    flw     f20,0x4C(sp)                                # Recover fs4
    flw     f21,0x50(sp)                                # Recover fs5
    flw     f22,0x54(sp)                                # Recover fs6
    flw     f23,0x58(sp)                                # Recover fs7
    flw     f24,0x5C(sp)                                # Recover fs8
    flw     f25,0x60(sp)                                # Recover fs9
    flw     f26,0x64(sp)                                # Recover fs10
    flw     f27,0x68(sp)                                # Recover fs11
    lw      t0, 0x6C(sp)                                # Recover fcsr
    csrw    fcsr, t0                                    #
#endif

    /* Recover standard preserved registers.  */
    /* Recover standard registers.  */

    lw      x1, 0x34(sp)                                # Recover RA
    lw      x8, 0x30(sp)                                # Recover s0
    lw      x9, 0x2C(sp)                                # Recover s1
    lw      x18, 0x28(sp)                               # Recover s2
    lw      x19, 0x24(sp)                               # Recover s3
    lw      x20, 0x20(sp)                               # Recover s4
    lw      x21, 0x1C(sp)                               # Recover s5
    lw      x22, 0x18(sp)                               # Recover s6
    lw      x23, 0x14(sp)                               # Recover s7
    lw      x24, 0x10(sp)                               # Recover s8
    lw      x25, 0x0C(sp)                               # Recover s9
    lw      x26, 0x08(sp)                               # Recover s10
    lw      x27, 0x04(sp)                               # Recover s11
    lw      t0, 0x38(sp)                                # Recover mstatus

#ifdef __riscv_flen
    addi    sp, sp, 116                                 # Recover stack frame
#else
    addi    sp, sp, 64                                  # Recover stack frame
#endif

    csrw    mstatus, t0                                 # Recover mstatus, may be enables interrupt, so must recover at last
    ret                                                 # Return to thread


_tx_thread_trigger_interrupt:
    call   trigger_sched_interrupt
_tx_thread_infinite_loop:
    /* Enable interrupts.  */
    csrsi   mstatus, 0x08                               # Enable interrupts
    j    _tx_thread_infinite_loop                       # While(1) loop to wait software interrupt trigger
/* }  */


.section .text.irq
.global eclic_msip_handler
eclic_msip_handler:
#ifdef __riscv_flen
    addi    sp, sp, -260                            	# Allocate space for all registers - with floating point enabled
#else
    addi    sp, sp, -128                            	# Allocate space for all registers - without floating point enabled
#endif

    /* First store t0, t1, t2 */
    sw      x5, 76(sp)                                  # Save t0
    sw      x6, 72(sp)                                  # Save t1
    sw      x7, 68(sp)                                  # Save t2


    lw      t1, _tx_thread_current_ptr                  # Pickup current thread pointer
    bnez    t1, _tx_thread_current_save                 # If not NULL, save current thread context and check preempt
#ifdef __riscv_flen
    addi    sp, sp, 260					# Else recover sp, go to check preempt
#else
    addi    sp, sp, 128
#endif
    j       _tx_thread_check_preempt                    # _tx_thread_current_ptr == NULL, go to check preempt

_tx_thread_current_save:
    /* save current thread context*/                     #
    li      t0, 1                                        # Build stack type
    sw      t0, 0(sp)                                    # Place stack type on the top
    sw      x27, 4(sp)                                   # Save s11
    sw      x26, 8(sp)                                   # Save s10
    sw      x25, 12(sp)                                  # Save s9
    sw      x24, 16(sp)                                  # Save s8
    sw      x23, 20(sp)                                  # Save s7
    sw      x22, 24(sp)                                  # Save s6
    sw      x21, 28(sp)                                  # Save s5
    sw      x20, 32(sp)                                  # Save s4
    sw      x19, 36(sp)                                  # Save s3
    sw      x18, 40(sp)                                  # Save s2
    sw      x9, 44(sp)                                   # Save s1
    sw      x8, 48(sp)                                   # Save s0
    sw      x31, 52(sp)                                  # Save t6
    sw      x30, 56(sp)                                  # Save t5
    sw      x29, 60(sp)                                  # Save t4
    sw      x28, 64(sp)                                  # Save t3
#   sw      x7, 68(sp)                                   # Save t2
#   sw      x6, 72(sp)                                   # Save t1
#   sw      x5, 76(sp)                                   # Save t0
    sw      x17, 80(sp)                                  # Save a7
    sw      x16, 84(sp)                                  # Save a6
    sw      x15, 88(sp)                                  # Save a5
    sw      x14, 92(sp)                                  # Save a4
    sw      x13, 96(sp)                                  # Save a3
    sw      x12, 100(sp)                                 # Save a2
    sw      x11, 104(sp)                                 # Save a1
    sw      x10, 108(sp)                                 # Save a0
    sw      x1, 112(sp)                                  # Save ra
    csrr    t0, mepc                                     # Load exception program counter
    sw      t0, 120(sp)                                 # Save it on the stack

#ifdef __riscv_flen
    fsw      f0, 124(sp)                                 # Save ft0
    fsw      f1, 128(sp)                                 # Save ft1
    fsw      f2, 132(sp)                                 # Save ft2
    fsw      f3, 136(sp)                                 # Save ft3
    fsw      f4, 140(sp)                                 # Save ft4
    fsw      f5, 144(sp)                                 # Save ft5
    fsw      f6, 148(sp)                                 # Save ft6
    fsw      f7, 152(sp)                                 # Save ft7
    fsw      f8, 156(sp)                                 # Save fs0
    fsw      f9, 160(sp)                                 # Save fs1
    fsw      f10, 164(sp)                                # Save fa0
    fsw      f11, 168(sp)                                # Save fa1
    fsw      f12, 172(sp)                                # Save fa2
    fsw      f13, 176(sp)                                # Save fa3
    fsw      f14, 180(sp)                                # Save fa4
    fsw      f15, 184(sp)                                # Save fa5
    fsw      f16, 188(sp)                                # Save fa6
    fsw      f17, 192(sp)                                # Save fa7
    fsw      f18, 196(sp)                                # Save fs2
    fsw      f19, 200(sp)                                # Save fs3
    fsw      f20, 204(sp)                                # Save fs4
    fsw      f21, 208(sp)                                # Save fs5
    fsw      f22, 212(sp)                                # Save fs6
    fsw      f23, 216(sp)                                # Save fs7
    fsw      f24, 220(sp)                                # Save fs8
    fsw      f25, 224(sp)                                # Save fs9
    fsw      f26, 228(sp)                                # Save fs10
    fsw      f27, 232(sp)                                # Save fs11
    fsw      f28, 236(sp)                                # Save ft8
    fsw      f29, 240(sp)                                # Save ft9
    fsw      f30, 244(sp)                                # Save ft10
    fsw      f31, 248(sp)                                # Save ft11
    csrr     t0, fcsr
    sw       t0, 252(sp)                                 # Store fcsr
    csrr     t0, mstatus
    sw       t0, 256(sp)                                 # Save mstatus
#else
    csrr     t0, mstatus
    sw       t0, 124(sp)                                 # Save mstatus
#endif

#if 0
    mv a0, sp
    call    checkSavedSpInMsft
    la      t0, _tx_thread_current_ptr                  # Pickup current thread pointer address
    lw      t1, 0(t0)                                   # Set current thread pointer
#endif

    sw      sp, 8(t1)                                   # Save current stack pointer*/
    /* Save the remaining time-slice and disable it.  */
    /* if (_tx_timer_time_slice)
    {  */

    la      t0, _tx_timer_time_slice                    # Pickup time slice variable address
    lw      t2, 0(t0)                                   # Pickup time slice
    beqz    t2, _tx_thread_check_preempt                # If 0, skip time slice processing

        /* _tx_thread_current_ptr -> tx_thread_time_slice =  _tx_timer_time_slice
        _tx_timer_time_slice =  0;  */

    sw      t2, 24(t1)                                  # Save current time slice
    sw      x0, 0(t0)                                   # Clear global time slice

    /* }  (_tx_timer_time_slice)*/


_tx_thread_check_preempt:
    call    clear_sw_irq				# Clear software interrupt flag

    lw      t2, _tx_thread_preempt_disable              # Pickup preempt disable flag
    bgtz    t2, _tx_thread_execute_restore          	# If disable preempt, restore currnet thread

#ifdef TX_ENABLE_EXECUTION_CHANGE_NOTIFY

    call    _tx_execution_thread_exit                   # Call the thread execution exit function
#endif
    /* Clear the current thread pointer.  */		# Else set current ptr to execute ptr
    /* _tx_thread_current_ptr =  TX_NULL;  */
    la      t0, _tx_thread_current_ptr                  # Pickup current thread pointer address
    sw      x0, 0(t0)                                   # Clear current thread pointer

    la      t0, _tx_thread_execute_ptr                  # Pickup address of execute ptr
    lw      t1, 0(t0)
    beqz    t1, _tx_thread_solicited_return             # If NULL, no preempt execute thread

    lw      sp, 8(t1)                                   # Pickup execute thread-s stack
    lw      t3, 0(sp)                                   # Pickup stack type
    beqz    t3, _tx_thread_solicited_return             # If 0, solicited thread return

    /* Setup the current thread pointer.  */
    /* _tx_thread_current_ptr =  _tx_thread_execute_ptr;  */

    la      t0, _tx_thread_current_ptr                  # Pickup current thread pointer address
    sw      t1, 0(t0)

    /* Increment the run count for this thread.  */
    /* _tx_thread_current_ptr -> tx_thread_run_count++;  */

    lw      t2, 4(t1)                                   # Pickup run count
    lw      t3, 24(t1)                                  # Pickup time slice value
    addi    t2, t2, 1                                   # Increment run count
    sw      t2, 4(t1)                                   # Store new run count


#ifdef TX_ENABLE_EXECUTION_CHANGE_NOTIFY
    call    _tx_execution_thread_enter                  # Call the thread execution enter function
#endif


_tx_thread_execute_restore:
    lw      t1, _tx_thread_current_ptr                  # Pickup current thread pointer
    beqz    t1, _tx_thread_solicited_return             # If NULL, go to solicited return

    lw      t3, 24(t1)                                  # Pickup time slice value
    /* Setup time-slice, if present.  */
    /* _tx_timer_time_slice =  _tx_thread_current_ptr -> tx_thread_time_slice;  */

    la      t2, _tx_timer_time_slice                    # Pickup time-slice variable address
    sw      t3, 0(t2)                                   # Store new time-slice*/

    lw      sp, 8(t1)                                   # Pickup current thread-s stack
#ifdef __riscv_flen
    flw     f0, 0x7C(sp)                                # Recover ft0
    flw     f1, 0x80(sp)                                # Recover ft1
    flw     f2, 0x84(sp)                                # Recover ft2
    flw     f3, 0x88(sp)                                # Recover ft3
    flw     f4, 0x8C(sp)                                # Recover ft4
    flw     f5, 0x90(sp)                                # Recover ft5
    flw     f6, 0x94(sp)                                # Recover ft6
    flw     f7, 0x98(sp)                                # Recover ft7
    flw     f8, 0x9C(sp)                                # Recover fs0
    flw     f9, 0xA0(sp)                                # Recover fs1
    flw     f10,0xA4(sp)                                # Recover fa0
    flw     f11,0xA8(sp)                                # Recover fa1
    flw     f12,0xAC(sp)                                # Recover fa2
    flw     f13,0xB0(sp)                                # Recover fa3
    flw     f14,0xB4(sp)                                # Recover fa4
    flw     f15,0xB8(sp)                                # Recover fa5
    flw     f16,0xBC(sp)                                # Recover fa6
    flw     f17,0xC0(sp)                                # Recover fa7
    flw     f18,0xC4(sp)                                # Recover fs2
    flw     f19,0xC8(sp)                                # Recover fs3
    flw     f20,0xCC(sp)                                # Recover fs4
    flw     f21,0xD0(sp)                                # Recover fs5
    flw     f22,0xD4(sp)                                # Recover fs6
    flw     f23,0xD8(sp)                                # Recover fs7
    flw     f24,0xDC(sp)                                # Recover fs8
    flw     f25,0xE0(sp)                                # Recover fs9
    flw     f26,0xE4(sp)                                # Recover fs10
    flw     f27,0xE8(sp)                                # Recover fs11
    flw     f28,0xEC(sp)                                # Recover ft8
    flw     f29,0xF0(sp)                                # Recover ft9
    flw     f30,0xF4(sp)                                # Recover ft10
    flw     f31,0xF8(sp)                                # Recover ft11
    lw      t0, 0xFC(sp)                                # Recover fcsr
    csrw    fcsr, t0                                    #
    lw      t0, 256(sp)                                 # Recover mstatus
#else
    lw      t0, 124(sp)                                 # Recover mstatus
#endif
    csrw    mstatus, t0                                 # Recover mstatus


    /* Recover standard registers.  */
    lw      t0, 120(sp)                                 # Recover mepc
    csrw    mepc, t0                                    # Store mepc

    lw      x27, 4(sp)                                   # Recover s11
    lw      x26, 8(sp)                                   # Recover s10
    lw      x25, 12(sp)                                  # Recover s9
    lw      x24, 16(sp)                                  # Recover s8
    lw      x23, 20(sp)                                  # Recover s7
    lw      x22, 24(sp)                                  # Recover s6
    lw      x21, 28(sp)                                  # Recover s5
    lw      x20, 32(sp)                                  # Recover s4
    lw      x19, 36(sp)                                  # Recover s3
    lw      x18, 40(sp)                                  # Recover s2
    lw      x9, 44(sp)                                   # Recover s1
    lw      x8, 48(sp)                                   # Recover s0
    lw      x31, 52(sp)                                  # Recover t6
    lw      x30, 56(sp)                                  # Recover t5
    lw      x29, 60(sp)                                  # Recover t4
    lw      x28, 64(sp)                                  # Recover t3
    lw      x7, 68(sp)                                   # Recover t2
    lw      x6, 72(sp)                                   # Recover t1
    lw      x5, 76(sp)                                   # Recover t0
    lw      x17, 80(sp)                                  # Recover a7
    lw      x16, 84(sp)                                  # Recover a6
    lw      x15, 88(sp)                                  # Recover a5
    lw      x14, 92(sp)                                  # Recover a4
    lw      x13, 96(sp)                                  # Recover a3
    lw      x12, 100(sp)                                 # Recover a2
    lw      x11, 104(sp)                                 # Recover a1
    lw      x10, 108(sp)                                 # Recover a0
    lw      x1, 112(sp)                                  # Recover ra

#ifdef __riscv_flen
    addi    sp, sp, 260                                 # Recover stack frame - with floating point registers
#else
    addi    sp, sp, 128                                 # Recover stack frame - without floating point registers
#endif

    mret

_tx_thread_solicited_return:
    la      t2,_tx_thread_system_stack_ptr              # Pickup stack pointer address
    lw      sp, 0(t2)                                   # Switch to system stack
    la      t0, _tx_thread_schedule        		# Pickup address of scheduling loop
    csrw    mepc, t0                                    # Store mepc
    mret

