#pragma once
#include <cspace/cspace.h>
#include <serial/serial.h>
#include <sos_coroutine.h>
extern struct serial* global_serial;
extern struct serial* global_debug_serial;

extern seL4_CPtr sos_syscall_cap;
extern seL4_CPtr sos_interrupt_cap;
