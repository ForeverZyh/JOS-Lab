#ifndef JOS_INC_MMU_H
#define JOS_INC_MMU_H

/*
 *
 *	Part 1.  Paging data structures and constants.
 *
 */

// A linear address 'la' has a three-part structure as follows:
//
// +--------12------+-------8--------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \---------- PGNUM(la) ----------/
//
// The PDX, PTX, PGOFF, and PGNUM macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// page number field of address
#define PGNUM(la)	(((uintptr_t) (la)) >> PTXSHIFT)

// page directory index
#define PDX(la)		((((uintptr_t) (la)) >> PDXSHIFT) & 0xFFF)

// page table index
#define PTX(la)		((((uintptr_t) (la)) >> PTXSHIFT) & 0xFF)

// offset in page
#define PGOFF(la)	(((uintptr_t) (la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d, t, o)	((void*) ((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
#define NPDENTRIES	4096		// page directory entries per page directory
#define NPTENTRIES	256		// page table entries per page table

#define PGSIZE		4096		// bytes mapped by a page
#define PDXSIZE		(NPDENTRIES * 4)
#define PGSHIFT		12		// log2(PGSIZE)

#define PTSIZE		(PGSIZE*NPTENTRIES) // bytes mapped by a page directory entry
#define PTSHIFT		20		// log2(PTSIZE)

#define PTXSHIFT	12		// offset of PTX in a linear address
#define PDXSHIFT	20		// offset of PDX in a linear address

// Page directory entry flags.
#define PDE_RDONLY (1 << 9)
#define PDE_APX (1 << 15)
#define PDE_APW (1 << 10)
#define PDE_APU (1 << 11)
#define PDE_ENTRY_1M (0x2)
#define PDE_ENTRY_16M ((0x2) | (1 << 18))
#define PDE_ENTRY (0x1)

#define PDE_P (0x3)

// Page table entry flags.
#define PTE_APX (1 << 9)
#define PTE_APW (1 << 4)
#define PTE_APU (1 << 5)
#define PTE_ENTRY_SMALL (0x2)
#define PTE_ENTRY_LARGE (0x1)

#define PTE_PR (0x3)

// Address in page table entry
#define PTE_ADDR(pte)	((physaddr_t) (pte) & ~0xFFF)
#define PDE_ADDR(pde)	((physaddr_t) (pde) & ~0x3FF)

#endif
