// Actually I copy & paste this code from http://wiki.osdev.org/ARM_RaspberryPi_Tutorial_C
#include <inc/mmu.h>
#include <inc/memlayout.h>
#define	RELOC(x) ((x) - KERNBASE)

.text

.globl		_start
_start = RELOC(entry)

.globl entry
entry:
	// Thanks CaoCao https://github.com/Friedrich1006/JOS/blob/arm
    // Check CPU ID
    mrc p15, 0, r0, c0, c0, 5
    tst r0, #3
    bne halt
	// Turn on MMU
	// Ref. http://www.embedded-bits.co.uk/2011/mmucode/
	// Set c2 to address of entry_pgdir
	ldr r0, =(entry_pgdir - KERNBASE)
	mcr p15, 0, r0, c2, c0, 0

	// make all domains 11 (Manager)
	mov r0, #0xFFFFFFFF
	mcr p15, 0, r0, c3, c0, 0

	// Set the last bit of c1 to 1
	mrc p15, 0, r0, c1, c0, 0
	orr r0, r0, #0x1
	mcr p15, 0, r0, c1, c0, 0
 
	// Jump up above KERNBASE
	ldr r3, =relocated
	blx r3
 
	// halt

relocated:
	mov r11, #0
	ldr sp, =bootstacktop  // Setup the stack.
	bl arm_init

halt:
	wfe
	b halt

.data
// boot stack
	.p2align        PGSHIFT         // force page alignment
	.globl          bootstack
bootstack:
	.space          KSTKSIZE
	.globl          bootstacktop
bootstacktop:

