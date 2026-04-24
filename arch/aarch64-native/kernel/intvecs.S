/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Licensed under the AROS Public License (APL), Version 1.1.

    Desc: AArch64 exception vector table (VBAR_EL1)
    Lang: english

    All exception handlers save a full ExceptionContext (272 bytes) on SP_EL1.
    The layout matches struct ExceptionContext field order:
      [SP+0..231]  r[0]-r[28]  (x0-x28, 29 regs × 8 bytes)
      [SP+232]     fp          (x29)
      [SP+240]     lr          (x30)
      [SP+248]     sp          (task's SP before exception)
      [SP+256]     pc          (ELR_EL1)
      [SP+264]     pstate      (SPSR_EL1)
    Total: 272 bytes = 34 × 8, 16-byte aligned.

    cpu_Switch() copies this frame to et_RegFrame.
    cpu_Dispatch() copies et_RegFrame back to this frame.
    The ASM then restores all registers and does eret.
*/

    .macro vector handler
    .align 7
    b       \handler
    .endm

    .text

    .align 11
    .globl VectorTable
VectorTable:
    /* Group 0: Current EL with SP_EL0 */
    vector  SynchronousStub
    vector  IRQStub
    vector  FIQStub
    vector  SErrorStub
    /* Group 1: Current EL with SP_ELx (EL1h) */
    vector  SynchronousStub
    vector  IRQStub
    vector  FIQStub
    vector  SErrorStub
    /* Group 2: Lower EL, AArch64 */
    vector  SynchronousStub
    vector  IRQStub
    vector  FIQStub
    vector  SErrorStub
    /* Group 3: Lower EL, AArch32 */
    vector  UnexpectedStub
    vector  UnexpectedStub
    vector  UnexpectedStub
    vector  UnexpectedStub

/*
 * Exception diagnostic stubs
 */
    .macro exc_stub name, exception
    .globl \name
\name:
    mrs     x0, esr_el1
    mrs     x1, spsr_el1
    mov     x2, x30
    mrs     x3, elr_el1
    mrs     x4, sp_el0
    mov     x5, sp
    mrs     x6, far_el1
    str     x6, [sp, #-16]!
    stp     x4, x5, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!
    stp     x0, x1, [sp, #-16]!
    mov     x0, #\exception
    mov     x1, sp
    bl      ExceptionHandler
    b       .
    .endm

    exc_stub    UnexpectedStub,     0
    exc_stub    SErrorStub,         3

/*
 * SAVE_FULL_CONTEXT — save all GPRs + ELR + SPSR to [SP-272].
 * After: SP points to saved ExceptionContext.
 */
.macro SAVE_FULL_CONTEXT
    stp     x0,  x1,  [sp, #-272]!
    stp     x2,  x3,  [sp, #16]
    stp     x4,  x5,  [sp, #32]
    stp     x6,  x7,  [sp, #48]
    stp     x8,  x9,  [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    str     x28,      [sp, #224]
    stp     x29, x30, [sp, #232]
    /* sp field: SP before we pushed = SP + 272 */
    add     x0, sp, #272
    str     x0,       [sp, #248]
    /* pc = ELR_EL1, pstate = SPSR_EL1 */
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0,  x1,  [sp, #256]
    /* Restore x0/x1 from frame (clobbered above) */
    ldp     x0,  x1,  [sp, #0]
.endm

/*
 * RESTORE_FULL_CONTEXT — restore all GPRs + ELR + SPSR from [SP], eret.
 *
 * After cpu_Dispatch, the frame at [SP] contains the NEW task's registers.
 * regs->sp has the new task's stack pointer.
 *
 * Strategy: use x0 as frame base. Load ELR/SPSR. Load all GPRs except x0.
 * Load SP from frame. Load x0 from [frame_base + 0]. eret.
 *
 * The trick: after loading SP, the old frame is still accessible via
 * the frame_base we saved in... wait, we used x0 for that and x0 is
 * about to be overwritten.
 *
 * Real trick: save frame base address in SP_EL0 (which we don't use
 * for anything else — AROS runs in EL1h). Then load x0 from SP_EL0-based
 * address after setting SP.
 */
.macro RESTORE_FULL_CONTEXT
    mov     x0, sp                  /* x0 = frame base */
    msr     sp_el0, x0              /* stash frame base in SP_EL0 */

    /* Load ELR and SPSR */
    ldp     x1,  x2,  [x0, #256]   /* x1=ELR, x2=SPSR */
    msr     elr_el1, x1
    msr     spsr_el1, x2

    /* Restore x1-x28 */
    ldr     x1,       [x0, #8]
    ldp     x2,  x3,  [x0, #16]
    ldp     x4,  x5,  [x0, #32]
    ldp     x6,  x7,  [x0, #48]
    ldp     x8,  x9,  [x0, #64]
    ldp     x10, x11, [x0, #80]
    ldp     x12, x13, [x0, #96]
    ldp     x14, x15, [x0, #112]
    ldp     x16, x17, [x0, #128]
    ldr     x18,      [x0, #144]
    ldp     x19, x20, [x0, #152]
    ldp     x21, x22, [x0, #168]
    ldp     x23, x24, [x0, #184]
    ldp     x25, x26, [x0, #200]
    ldp     x27, x28, [x0, #216]
    ldp     x29, x30, [x0, #232]

    /* Load task SP and switch */
    ldr     x0,       [x0, #248]
    mov     sp, x0

    /* Restore x0 from stashed frame base */
    mrs     x0, sp_el0              /* x0 = frame base */
    ldr     x0, [x0, #0]           /* x0 = saved x0 */

    eret
.endm

/*
 * SynchronousStub — SVC and other synchronous exceptions.
 */
    .globl SynchronousStub
SynchronousStub:
    /* Check if SVC (ESR_EL1.EC == 0x15) */
    mrs     x0, esr_el1
    lsr     x1, x0, #26
    cmp     x1, #0x15
    b.ne    .Lsync_not_svc

    /* Extract syscall number from ISS[15:0] */
    and     x0, x0, #0xFFFF

    /* For non-switch syscalls, handle quickly */
    cmp     x0, #0                  /* SC_CAUSE */
    b.eq    .Lsvc_cause
    cmp     x0, #1                  /* SC_DISPATCH */
    b.eq    .Lsvc_switch_path
    cmp     x0, #2                  /* SC_SWITCH */
    b.eq    .Lsvc_switch_path
    cmp     x0, #3                  /* SC_SCHEDULE */
    b.eq    .Lsvc_switch_path

    /* Unknown syscall — just return */
    eret

.Lsvc_cause:
.Lsvc_switch_path:
    /*
     * Save full context and call core_SysCall(sc, regs).
     * core_SysCall handles SC_CAUSE/SC_SCHEDULE/SC_SWITCH/SC_DISPATCH
     * via cpu_Switch/cpu_Dispatch which modify the regs in-place.
     */
    SAVE_FULL_CONTEXT

    /* Re-extract syscall number from ESR_EL1 (save_context clobbered x0) */
    mrs     x9, esr_el1
    and     x9, x9, #0xFFFF

    /* Bump SupervisorCount */
    mrs     x2, tpidr_el1
    ldrb    w3, [x2, #30]          /* tls->SupervisorCount */
    add     w3, w3, #1
    strb    w3, [x2, #30]

    /* core_SysCall(sc, regs) */
    mov     x0, x9                  /* syscall number */
    mov     x1, sp                  /* regs = frame on stack */
    bl      core_SysCall

    /* Mask IRQs before decrementing SupervisorCount.
     * Without this, an IRQ between the decrement and eret could
     * trigger core_ExitInterrupt which overwrites the regs frame
     * with a different task's context. */
    msr     daifset, #2

    /* Decrement SupervisorCount */
    mrs     x2, tpidr_el1
    ldrb    w3, [x2, #30]
    sub     w3, w3, #1
    strb    w3, [x2, #30]

    RESTORE_FULL_CONTEXT

.Lsync_not_svc:
    /*
     * Non-SVC synchronous exception (data abort, etc.)
     * Save full context, call ExceptionHandler.
     */
    SAVE_FULL_CONTEXT

    mov     x0, #1                  /* exception type = synchronous */
    mov     x1, sp                  /* frame pointer */
    bl      ExceptionHandler

    cbz     x0, .Lexc_halt

    RESTORE_FULL_CONTEXT

.Lexc_halt:
    wfe
    b       .Lexc_halt

/*
 * IRQStub — save full context, handle IRQ, check for reschedule.
 */
    .globl IRQStub
IRQStub:
    SAVE_FULL_CONTEXT

    /* Bump SupervisorCount */
    mrs     x2, tpidr_el1
    ldrb    w3, [x2, #30]
    add     w3, w3, #1
    strb    w3, [x2, #30]

    bl      InterruptHandler

    /*
     * Only call core_ExitInterrupt if we interrupted a task (not kernel code).
     * Check SupervisorCount: if > 1, we're inside an SVC handler or nested
     * exception. The idle loop in cpu_Dispatch handles scheduling via
     * FLAG_SCHEDSWITCH, so we only need core_ExitInterrupt for IRQs that
     * interrupted normal task execution (SupervisorCount == 1).
     */
    mrs     x2, tpidr_el1
    ldrb    w3, [x2, #30]          /* SupervisorCount */
    cmp     w3, #1
    b.hi    .Lirq_no_schedule

    mov     x0, sp
    bl      core_ExitInterrupt

.Lirq_no_schedule:
    /* Decrement SupervisorCount */
    mrs     x2, tpidr_el1
    ldrb    w3, [x2, #30]
    sub     w3, w3, #1
    strb    w3, [x2, #30]

    RESTORE_FULL_CONTEXT

/*
 * FIQ stub — not used, just return.
 */
    .globl FIQStub
FIQStub:
    eret
