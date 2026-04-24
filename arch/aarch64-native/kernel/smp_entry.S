/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Secondary core startup for AArch64 SMP (spin-table method).
    Lang: english

    Secondary cores enter at smp_entry in EL2 (QEMU) or EL1.
    We do EL2→EL1 transition, set up per-core state, and enter idle loop.

    Before writing our address to cpu-release-addr, the C code stores
    per-core parameters at a known location:
      smp_boot_stack  — per-core SP_EL1
      smp_boot_tls    — per-core TLS pointer (tls_t *)
      smp_boot_vbar   — VBAR_EL1 value (shared)
*/

    .section .text
    .balign 64
    .globl smp_entry
smp_entry:
    /*
     * Check current EL. If EL2, transition to EL1.
     * If already EL1, skip.
     */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    b.ne    .Lsmp_el1

    /* EL2 → EL1 transition (same as bootstrap) */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #0x3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    mrs     x0, midr_el1
    mrs     x1, mpidr_el1
    msr     vpidr_el2, x0
    msr     vmpidr_el2, x1

    mov     x0, #0x33FF
    msr     cptr_el2, x0
    msr     hstr_el2, xzr

    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0

    mov     x0, #(1 << 31)
    msr     hcr_el2, x0

    mov     x0, #0x0800
    movk    x0, #0x30D0, lsl #16
    msr     sctlr_el1, x0

    mov     x0, #0x3C5             /* EL1h, DAIF masked */
    msr     spsr_el2, x0
    adr     x0, .Lsmp_el1
    msr     elr_el2, x0
    eret

.Lsmp_el1:
    /* Now in EL1. Use SP_EL1. */
    msr     spsel, #1

    /* Load per-core boot parameters */
    adrp    x0, smp_boot_stack
    ldr     x1, [x0, #:lo12:smp_boot_stack]
    mov     sp, x1

    adrp    x0, smp_boot_tls
    ldr     x1, [x0, #:lo12:smp_boot_tls]
    msr     tpidr_el1, x1

    adrp    x0, smp_boot_vbar
    ldr     x1, [x0, #:lo12:smp_boot_vbar]
    msr     vbar_el1, x1
    isb

    /* Enable FP/SIMD */
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    /* Call C entry point */
    bl      smp_secondary_init

    /* Should not return — idle loop */
1:  wfe
    b       1b

    /* Per-core boot parameters — written by C code before waking each core */
    .data
    .balign 8
    .globl smp_boot_stack
smp_boot_stack:
    .quad   0
    .globl smp_boot_tls
smp_boot_tls:
    .quad   0
    .globl smp_boot_vbar
smp_boot_vbar:
    .quad   0
