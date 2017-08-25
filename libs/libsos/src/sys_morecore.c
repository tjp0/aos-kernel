/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <ipc.h>
#include <syscall.h>
#include <sos.h>

/* I accidentally wrote sbrk instead of brk */
static long sys_sbrk_internal(uint32_t size) {
    struct ipc_command ipc = ipc_create();
    ipc_packi(&ipc, SOS_SYSCALL_SBRK);
    ipc_packi(&ipc, size);
    uint32_t ret = ipc_call(&ipc, SOS_IPC_EP_CAP);
    return ret;
}

/* Wrapper to make brk use sbrk */
static long dbrk = 0;
long
sys_brk_internal(uintptr_t newbrk) {
/*if the newbrk is 0, return the bottom of the heap*/

    if(dbrk == 0) {
        dbrk = sys_sbrk_internal(0);
    }

    if(newbrk == 0) {
        return dbrk;
    } else {
        long ret = sys_sbrk_internal(newbrk-dbrk);
        if(ret == 0) {
            return 0;
        }
        return newbrk;
    }
}

long
sys_brk(va_list ap)
{
    uintptr_t newbrk = va_arg(ap, uintptr_t);
    return sys_brk_internal(newbrk);
}

/* Large mallocs will result in muslc calling mmap, so we do a minimal implementation
   here to support that. We make a bunch of assumptions in the process */
long
sys_mmap2(va_list ap)
{
    /*
    void *addr = va_arg(ap, void*);
    size_t length = va_arg(ap, size_t);
    int prot = va_arg(ap, int);
    int flags = va_arg(ap, int);
    int fd = va_arg(ap, int);
    off_t offset = va_arg(ap, off_t);
    (void)addr;
    (void)prot;
    (void)fd;
    (void)offset;
    assert(!"not implemented");
    */
    printf("Oh noe");
    return -ENOMEM;
}

long
sys_mremap(va_list ap)
{
    assert(!"not implemented");
    return -ENOMEM;
}
