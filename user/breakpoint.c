// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("before break point!\n");
	int b = 10;
	asm volatile("int $3");
	int a = 1;
	cprintf("%d\n", a + b);
	b = 0;
	cprintf("%d\n", a + b);
	cprintf("end!\n");
}

