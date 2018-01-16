#ifndef JOS_INC_ARM_H
#define JOS_INC_ARM_H

#include <inc/types.h>

static inline uint32_t
read_ebp(void)
{
	uint32_t ebp;
	asm volatile("mov %0, r11" : "=r" (ebp));
	return ebp;
}

static inline void
lcr3(uint32_t val)
{
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r"(val));
}

static inline uint32_t
rcr3(void)
{
	uint32_t val;
	asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r" (val));
	return val;
}

#endif
