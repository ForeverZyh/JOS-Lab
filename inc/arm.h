static inline uint32_t
read_ebp(void)
{
	uint32_t ebp;
	asm volatile("mov %0, r11" : "=r" (ebp));
	return ebp;
}