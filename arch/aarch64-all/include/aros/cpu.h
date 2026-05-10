#ifndef AROS_AARCH64_CPU_H
#define AROS_AARCH64_CPU_H

/*
    Copyright © 2016, The AROS Development Team. All rights reserved.
    $Id$

    NOTE: This file must compile *without* any other header !

    Desc: cpu.h include file for aarch64-le systems
    Lang: english
*/

/* Translate __ARM_ARCH_X__ definitions to a common one. Makes it easier to write conditional code. */
#ifdef __ARM_ARCH_8A__
#define __ARM_ARCH__ 8
#endif

typedef	unsigned int	cpuid_t;
typedef	unsigned int	cpumask_t;

/* Information about size and alignment,
 * the defines have to be numeric constants */
#define AROS_STACK_GROWS_DOWNWARDS 1 /* Stack direction */
#define AROS_BIG_ENDIAN 	   0 /* Big or little endian */
#define AROS_SIZEOFULONG	   4 /* Size of an ULONG */
#define AROS_SIZEOFPTR		   8 /* Size of a PTR */
#define AROS_WORDALIGN		   2 /* Alignment for WORD */
#define AROS_LONGALIGN		   4 /* Alignment for LONG */
#define AROS_QUADALIGN		   8 /* Alignment for QUAD */
#define AROS_PTRALIGN		   8 /* Alignment for PTR */
#define AROS_IPTRALIGN		   8 /* Alignment for IPTR */
#define AROS_DOUBLEALIGN	   8 /* Alignment for double */
#define AROS_WORSTALIGN 	  16 /* Worst case alignment */
#define AROS_STACKALIGN		  16 /* FIXME: is this really needed? */

/* define this if we have no support for linear varargs in the compiler */
#define NO_LINEAR_VARARGS       1

/* Do not use patched compiler. */
#define AROS_SLOWSTACKTAGS      1
#define AROS_SLOWSTACKMETHODS   1
#define AROS_SLOWSTACKHOOKS     1
#define AROS_SLOWSTACKFORMAT    1

#define AROS_32BIT_TYPE         int
#define AROS_64BIT_TYPE         long
#define AROS_64BIT_STACKTYPE    long

#define AROS_MAKE_INT64(i)  i ## L
#define AROS_MAKE_UINT64(i) i ## UL

#define __WORDSIZE              64

#define STACKED __attribute__((aligned(8)))

/* Use C pointer and string for the BCPL pointers and strings
 * For a normal ABI these should not be defined for maximum source code
 * compatibility.
 */
#define AROS_FAST_BPTR 1
#define AROS_FAST_BSTR 1

/* do we need a function attribute to get parameters on the stack? */
#define __stackparm

/* types and limits for sig_atomic_t */
#define AROS_SIG_ATOMIC_T       int
#define AROS_SIG_ATOMIC_MIN     (-0x7fffffff-1)
#define AROS_SIG_ATOMIC_MAX     0x7fffffff

#if defined(__GNUC__) && !defined(__clang__)
register unsigned char* AROS_GET_SP __asm__("%sp");
#endif

/*
    One entry in a libraries' jumptable. For assembler compatibility, the
    fields contain AArch64 instructions that perform an absolute jump to
    a 64-bit address:
        ldr x16, .+8       (0x58000050)  — load target from next 8 bytes
        br  x16             (0xd61f0200)  — branch to target
        .quad target_addr                 — 64-bit target address

    DECISION: x16 (ip0) is used as the scratch register because AAPCS64
    designates x16/x17 as intra-procedure-call scratch registers that
    callers must not rely on being preserved.
    Date: 2026-04-10

    On darwin-aarch64, W^X enforcement prevents writing executable code
    to RAM. FullJumpVec is only used for segment entry points. The
    AROS_SET_FULLJMP macro writes the trampoline and then calls
    CacheClearE to flush the icache and mprotect the page to RX.
*/
/*
 * On darwin-aarch64, W^X enforcement prevents writing executable code
 * to RAM. FullJumpVec is redefined as a simple function pointer (same
 * as JumpVec). The segment entry point is called indirectly by reading
 * the pointer, not by executing a trampoline.
 */
#if defined(HOST_OS_darwin) && defined(__aarch64__)
struct FullJumpVec
{
    void *target;
};
#define __AROS_SET_FULLJMP(v,a) \
do { \
    ((struct FullJumpVec *)(v))->target = (void *)(a); \
} while (0)
#define __AROS_GET_FULLJMP_TARGET(v) (((struct FullJumpVec *)(v))->target)

/* Get the entry point from a segment. On darwin-aarch64, FullJumpVec is a
 * data pointer that must be dereferenced. On other platforms, the segment
 * data IS the entry point (executable trampoline). */
#define __AROS_SEG_ENTRY(seg) __AROS_GET_FULLJMP_TARGET((UBYTE *)BADDR(seg) + sizeof(BPTR))
#else
struct FullJumpVec
{
    unsigned int jmp[2];    /* Two 32-bit AArch64 instructions          */
    unsigned long vec;      /* 64-bit target address                    */
};
#define __AROS_SET_FULLJMP(v,a) \
do \
{  \
    struct FullJumpVec *_v = (v); \
    _v->jmp[0] = 0x58000050;   /* ldr x16, .+8             */  \
    _v->jmp[1] = 0xd61f0200;   /* br  x16                  */  \
    _v->vec    = (unsigned long)(a); /* 64-bit target address */  \
} while (0)
#define __AROS_GET_FULLJMP_TARGET(v) ((void *)(((struct FullJumpVec *)(v))->vec))
#define __AROS_SEG_ENTRY(seg) ((APTR)((UBYTE *)BADDR(seg) + sizeof(BPTR)))
#endif

/*
 * W^X support macros for macOS arm64 (Apple Silicon).
 *
 * macOS enforces that MAP_JIT pages can only be writable OR executable
 * at any given time per thread. pthread_jit_write_protect_np(false)
 * makes them writable; pthread_jit_write_protect_np(true) makes them
 * executable again. sys_icache_invalidate() is needed after writing
 * instructions to ensure the icache sees the new code.
 *
 * On all other platforms these are no-ops.
 */
/*
 * W^X support for macOS arm64 (Apple Silicon).
 *
 * macOS enforces that MAP_JIT pages can only be writable OR executable
 * at any given time per thread. During boot, the bootstrap puts RAM in
 * write mode. After InitCode writes all FullJumpVecs, the kernel
 * switches to execute mode. Individual FullJumpVec writes during
 * runtime will need proper W^X toggling through the host interface.
 *
 * For now, the macros are no-ops. The kernel handles the global toggle.
 */
#define AROS_BEGIN_CODE_WRITE()  do { } while (0)
#define AROS_END_CODE_WRITE()    do { } while (0)

struct JumpVec
{
	void * vec;
};

/* Use these to acces a vector table */
#define LIB_VECTSIZE			((int)sizeof (struct JumpVec))
#define __AROS_GETJUMPVEC(lib,n)        (&((struct JumpVec *)lib)[-(long)(n)])
#define __AROS_GETVECADDR(lib,n)        (__AROS_GETJUMPVEC(lib,n)->vec)
#define __AROS_SETVECADDR(lib,n,addr)   (__AROS_GETJUMPVEC(lib,n)->vec = (addr))
#define __AROS_INITVEC(lib,n)		__AROS_SETVECADDR(lib,n,_aros_not_implemented)

/* Register preservation is still useful. At least for stack swapping functions. */
#undef UseExecstubs
#define UseExecstubs 1

/* Macros to test/set failure of AllocEntry() */
#define AROS_ALLOCENTRY_FAILED(memType) \
	((struct MemList *)((IPTR)(memType) | 0x80ul<<(sizeof(APTR)-1)*8))
#define AROS_CHECK_ALLOCENTRY(memList) \
	(!((IPTR)(memList) & 0x80ul<<(sizeof(APTR)-1)*8))

/*
    Find the next valid alignment for a structure if the next x bytes must
    be skipped.
*/
#define AROS_ALIGN(x)        (((x)+AROS_WORSTALIGN-1)&-AROS_WORSTALIGN)

/* Prototypes */
extern void _aros_not_implemented (char *);

/* How much stack do we need ? Lots :-) */
#define AROS_STACKSIZE	40960

/* How to map function arguments to CPU registers */
/*
    The ARM processor does have enough registers to map the m68k
    register set onto them but we will ignore this and use the compiler's calling
    convention. The library base is mapped to the last argument so that
    it can be ignored by the function.
*/

/* What to do with the library base in header, prototype and call */
#define __AROS_LH_BASE(basetype,basename)   basetype basename
#define __AROS_LP_BASE(basetype,basename)   void *
#define __AROS_LC_BASE(basetype,basename)   basename
#define __AROS_LD_BASE(basetype,basename)   basetype

/* How to transform an argument in header, opt prototype, call and forced
   prototype. */
#define __AROS_LHA(type,name,reg)     type name
#define __AROS_LPA(type,name,reg)     type
#define __AROS_LCA(type,name,reg)     name
#define __AROS_LDA(type,name,reg)     type
#define __AROS_UFHA(type,name,reg)    type name
#define __AROS_UFPA(type,name,reg)    type
#define __AROS_UFCA(type,name,reg)    name
#define __AROS_UFDA(type,name,reg)    type
#define __AROS_LHA2(type,name,reg1,reg2)     type name
#define __AROS_LPA2(type,name,reg1,reg2)     type
#define __AROS_LCA2(type,name,reg1,reg2)     name
#define __AROS_LDA2(type,name,reg1,reg2)     type

/* Prefix for library function in header, prototype and call */
#define __AROS_LH_PREFIX    /* eps */
#define __AROS_LP_PREFIX    /* eps */
#define __AROS_LC_PREFIX    /* eps */
#define __AROS_LD_PREFIX    /* eps */
#define __AROS_UFH_PREFIX   /* eps */
#define __AROS_UFP_PREFIX   /* eps */
#define __AROS_UFC_PREFIX   /* eps */
#define __AROS_UFD_PREFIX   /* eps */

#endif /* AROS_AARCH64_CPU_H */

