/*!
    \file    entry.S
    \brief   Entry for interrupts for GD32VW55x SDK.

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

#include "riscv_encoding.h"

/**
 * \brief  Macro for context save
 * \details
 * This macro save ABI defined caller saved registers in the stack.
 * \remarks
 * - This Macro could use to save context when you enter to interrupt
 * or exception
*/
/* Save caller registers */
.macro SAVE_CONTEXT
    /* Allocate stack space for context saving */
#ifndef __riscv_32e
    addi sp, sp, -20*REGBYTES
#else
    addi sp, sp, -14*REGBYTES
#endif /* __riscv_32e */

    STORE x1, 0*REGBYTES(sp)
    STORE x4, 1*REGBYTES(sp)
    STORE x5, 2*REGBYTES(sp)
    STORE x6, 3*REGBYTES(sp)
    STORE x7, 4*REGBYTES(sp)
    STORE x10, 5*REGBYTES(sp)
    STORE x11, 6*REGBYTES(sp)
    STORE x12, 7*REGBYTES(sp)
    STORE x13, 8*REGBYTES(sp)
    STORE x14, 9*REGBYTES(sp)
    STORE x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
    STORE x16, 14*REGBYTES(sp)
    STORE x17, 15*REGBYTES(sp)
    STORE x28, 16*REGBYTES(sp)
    STORE x29, 17*REGBYTES(sp)
    STORE x30, 18*REGBYTES(sp)
    STORE x31, 19*REGBYTES(sp)
#endif /* __riscv_32e */

    /* Save the necessary CSR registers */
    SAVE_CSR_CONTEXT

#ifdef __riscv_flen
    addi  sp, sp, -1*REGBYTES
    csrr  t0, CSR_FCSR
    STORE t0, 0*REGBYTES(sp)

    addi sp, sp, -20*FPREGBYTES
    FPSTORE f0, 0*FPREGBYTES(sp)
    FPSTORE f1, 1*FPREGBYTES(sp)
    FPSTORE f2, 2*FPREGBYTES(sp)
    FPSTORE f3, 3*FPREGBYTES(sp)
    FPSTORE f4, 4*FPREGBYTES(sp)
    FPSTORE f5, 5*FPREGBYTES(sp)
    FPSTORE f6, 6*FPREGBYTES(sp)
    FPSTORE f7, 7*FPREGBYTES(sp)
    FPSTORE f10, 8*FPREGBYTES(sp)
    FPSTORE f11, 9*FPREGBYTES(sp)
    FPSTORE f12, 10*FPREGBYTES(sp)
    FPSTORE f13, 11*FPREGBYTES(sp)
    FPSTORE f14, 12*FPREGBYTES(sp)
    FPSTORE f15, 13*FPREGBYTES(sp)
    FPSTORE f16, 14*FPREGBYTES(sp)
    FPSTORE f17, 15*FPREGBYTES(sp)
    FPSTORE f28, 16*FPREGBYTES(sp)
    FPSTORE f29, 17*FPREGBYTES(sp)
    FPSTORE f30, 18*FPREGBYTES(sp)
    FPSTORE f31, 19*FPREGBYTES(sp)
#endif
.endm

/**
 * \brief  Macro for restore caller registers
 * \details
 * This macro restore ABI defined caller saved registers from stack.
 * \remarks
 * - You could use this macro to restore context before you want return
 * from interrupt or exeception
 */
/* Restore caller registers */
.macro RESTORE_CONTEXT
#ifdef __riscv_flen
    /* Restore fp caller registers */
    FPLOAD f0, 0*FPREGBYTES(sp)
    FPLOAD f1, 1*FPREGBYTES(sp)
    FPLOAD f2, 2*FPREGBYTES(sp)
    FPLOAD f3, 3*FPREGBYTES(sp)
    FPLOAD f4, 4*FPREGBYTES(sp)
    FPLOAD f5, 5*FPREGBYTES(sp)
    FPLOAD f6, 6*FPREGBYTES(sp)
    FPLOAD f7, 7*FPREGBYTES(sp)
    FPLOAD f10, 8*FPREGBYTES(sp)
    FPLOAD f11, 9*FPREGBYTES(sp)
    FPLOAD f12, 10*FPREGBYTES(sp)
    FPLOAD f13, 11*FPREGBYTES(sp)
    FPLOAD f14, 12*FPREGBYTES(sp)
    FPLOAD f15, 13*FPREGBYTES(sp)
    FPLOAD f16, 14*FPREGBYTES(sp)
    FPLOAD f17, 15*FPREGBYTES(sp)
    FPLOAD f28, 16*FPREGBYTES(sp)
    FPLOAD f29, 17*FPREGBYTES(sp)
    FPLOAD f30, 18*FPREGBYTES(sp)
    FPLOAD f31, 19*FPREGBYTES(sp)
    addi sp, sp, 20*FPREGBYTES

    LOAD t0, 0*REGBYTES(sp)
    csrw CSR_FCSR, t0
    addi sp, sp, 1*REGBYTES
#endif

    /* Restore the necessary CSR registers */
    RESTORE_CSR_CONTEXT

    LOAD x1, 0*REGBYTES(sp)
    LOAD x4, 1*REGBYTES(sp)
    LOAD x5, 2*REGBYTES(sp)
    LOAD x6, 3*REGBYTES(sp)
    LOAD x7, 4*REGBYTES(sp)
    LOAD x10, 5*REGBYTES(sp)
    LOAD x11, 6*REGBYTES(sp)
    LOAD x12, 7*REGBYTES(sp)
    LOAD x13, 8*REGBYTES(sp)
    LOAD x14, 9*REGBYTES(sp)
    LOAD x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
    LOAD x16, 14*REGBYTES(sp)
    LOAD x17, 15*REGBYTES(sp)
    LOAD x28, 16*REGBYTES(sp)
    LOAD x29, 17*REGBYTES(sp)
    LOAD x30, 18*REGBYTES(sp)
    LOAD x31, 19*REGBYTES(sp)

    /* De-allocate the stack space */
    addi sp, sp, 20*REGBYTES
#else
    /* De-allocate the stack space */
    addi sp, sp, 14*REGBYTES
#endif /* __riscv_32e */
.endm

/**
 * \brief  Macro for save necessary CSRs to stack
 * \details
 * This macro store MCAUSE, MEPC, MSUBM to stack.
 */
.macro SAVE_CSR_CONTEXT
    /* Store CSR mcause to stack using pushmcause */
    csrrwi  x0, CSR_PUSHMCAUSE, 11
    /* Store CSR mepc to stack using pushmepc */
    csrrwi  x0, CSR_PUSHMEPC, 12
    /* Store CSR msub to stack using pushmsub */
    csrrwi  x0, CSR_PUSHMSUBM, 13
.endm

/**
 * \brief  Macro for restore necessary CSRs from stack
 * \details
 * This macro restore MSUBM, MEPC, MCAUSE from stack.
 */
.macro RESTORE_CSR_CONTEXT
    LOAD x5,  13*REGBYTES(sp)
    csrw CSR_MSUBM, x5
    LOAD x5,  12*REGBYTES(sp)
    csrw CSR_MEPC, x5
    LOAD x5,  11*REGBYTES(sp)
    csrw CSR_MCAUSE, x5
.endm

/**
 * \brief  Exception/NMI Entry
 * \details
 * This function provide common entry functions for exception/nmi.
 * \remarks
 * This function provide a default exception/nmi entry.
 * ABI defined caller save register and some CSR registers
 * to be saved before enter interrupt handler and be restored before return.
 */
.section .text.trap
/* In CLIC mode, the exeception entry must be 64bytes aligned */
.align 6
.global exc_entry
.weak exc_entry
exc_entry:
    /* Save the caller saving registers (context) */
    SAVE_CONTEXT

    /*
     * Set the exception handler function arguments
     * argument 1: mcause value
     * argument 2: current stack point(SP) value
     */
    csrr a0, mcause
    mv a1, sp
    /*
     * TODO: Call the exception handler function
     * By default, the function template is provided in
     * system_gd32vw55x.c, you can adjust it as you want
     */
    call core_exception_handler

    /* Restore the caller saving registers (context) */
    RESTORE_CONTEXT

    /* Return to regular code */
    mret

/**
 * \brief  Non-Vector Interrupt Entry
 * \details
 * This function provide common entry functions for handling
 * non-vector interrupts
 * \remarks
 * This function provide a default non-vector interrupt entry.
 * ABI defined caller save register and some CSR registers need
 * to be saved before enter interrupt handler and be restored before return.
 */
.section      .text.irq
/* In CLIC mode, the interrupt entry must be 4bytes aligned */
.align 2
.global irq_entry
.weak irq_entry
/* This label will be set to MTVT2 register */
irq_entry:
    /* Before calling _tx_thread_context_save, we have to allocate an interrupt
       stack frame and save the current value of x1 (ra). */
#ifdef __riscv_flen
    addi    sp, sp, -260                            # Allocate space for all registers - with floating point enabled
#else
    addi    sp, sp, -128                            # Allocate space for all registers - without floating point enabled
#endif

    sw      x1, 0x70(sp)                            # Store RA
    call    _tx_thread_context_save                 # Call ThreadX context save
    /* This special CSR read/write operation, which is actually
     * claim the CLIC to find its pending highest ID, if the ID
     * is not 0, then automatically enable the mstatus.MIE, and
     * jump to its vector-entry-label, and update the link register
     */

    csrrw ra, CSR_JALMNXTI, ra

    /* Timer interrupt processing is done, jump to ThreadX context restore.  */
    call	_tx_thread_context_restore              # Jump to ThreadX context restore function. Note: this does not return!


/* Default Handler for Exceptions / Interrupts */
.global default_intexc_handler
.weak default_intexc_handler
Undef_Handler:
default_intexc_handler:
1:
    j 1b
