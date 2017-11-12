// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW)))
		panic("not a write, nor not PTE_COW");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);
	//cprintf("addr: %08x\n", addr);
	//envid_t envid = sys_getenvid();
	//cprintf("%d\n",(int)envid);
	if ((r = sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P)) < 0)
		panic("pgfault: sys_page_alloc %e", r);
	memcpy((void*) PFTEMP, addr, PGSIZE);
	if ((r = sys_page_map(0, (void*) PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P)) < 0)
		panic("pgfault: sys_page_map %e", r);
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("pgfault: sys_page_unmap %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	uint32_t addr = pn * PGSIZE;
	int error_code = 0;
	//cprintf("%08x\n", addr);
	if (uvpt[pn] & (PTE_W | PTE_COW))
	{
		if ((error_code = sys_page_map(0, (void*) addr, envid, (void*) addr, PTE_COW | PTE_U | PTE_P)) < 0)
			panic("duppage: COW, 0 -> envid %e!", error_code);
		if ((error_code = sys_page_map(0, (void*) addr, 0, (void*) addr, PTE_COW | PTE_U | PTE_P)) < 0)
			panic("duppage: COW, 0 -> 0 %e!", error_code);
		//cprintf("enen\n");
	}
	else
	{
		if (sys_page_map(0, (void*) addr, envid, (void*) addr, PTE_U | PTE_P) < 0)
			panic("duppage: read-only, 0 -> envid %e!", error_code);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("fork: sys_exofork error!");
	else if (envid == 0)
	{
		thisenv = envs + ENVX(sys_getenvid());
		return 0;
	}
	for(uint32_t i = 0;i < USTACKTOP;i += PGSIZE)
	{
		if ((uvpd[PDX(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_U))
			duppage(envid, PGNUM(i));
	}
	//panic("here");
	if (sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P) < 0)
		panic("fork: sys_page_map on UXSTACKTOP");
	if (sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall) < 0)
		panic("fork: sys_env_set_pgfault_upcall");
	if (sys_env_set_status(envid, ENV_RUNNABLE) < 0)
		panic("fork: sys_env_set_status");
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
