#pragma once
#include <autoconf.h>
#include <cspace/cspace.h>
#include <serial/serial.h>
#include <sos_coroutine.h>
extern struct serial* global_serial;

#ifdef CONFIG_SOS_DEBUG_NETWORK
extern struct serial* global_debug_serial;
#endif

extern seL4_CPtr sos_syscall_cap;
extern seL4_CPtr sos_interrupt_cap;
