#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>
#include <kernel_base.h>

AROS_LH1(void, KrnSpinInit,
    AROS_LHA(spinlock_t *, lock, A1),
    struct KernelBase *, KernelBase, 49, Kernel)
{
    AROS_LIBFUNC_INIT
    lock->lock = 0;
    AROS_LIBFUNC_EXIT
}
