#pragma once
#include <sel4/sel4.h>

seL4_Word ft_early_initialize(const seL4_BootInfo* bootinfo);
void ft_late_initialize(seL4_Word base);

void frame_test(void);