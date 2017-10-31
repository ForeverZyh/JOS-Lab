// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
static bool enable_single_step = false;
struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace", mon_backtrace },
	{ "showmappings", "Display the physical page mappings and corresponding permission bits", mon_showmappings},
	{ "setperms", "Set the perm of the page of that virutal address", mon_setperms},
	{ "dump", "Dump the pages of that virutal/physical address", mon_dump},
	{ "lookmem", "Look up some bits/bytes in that virutal/physical address", mon_lookmem},
	{ "s", "Single step to next instruction", mon_singlestep},
	{ "c", "Continue execution", mon_continue},
	
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint32_t ebp = read_ebp();
	uint32_t esp;
	cprintf("Stack backtrace:\n");
	while (ebp)
	{
		cprintf("  ebp %08x",ebp);
		uint32_t eip=*((int*)ebp+1);
		cprintf("  eip %08x",eip);
		cprintf("  args");
		for(int i=0;i<5;i++)
			cprintf(" %08x",*((int*)ebp+2+i));
		cprintf("\n");
		struct Eipdebuginfo info;
		int flag = debuginfo_eip(eip,&info);
		if (flag==0)
		{
			cprintf("       %s:%d: ",info.eip_file,info.eip_line);
			for(int i=0;i<info.eip_fn_namelen;i++) cprintf("%c",info.eip_fn_name[i]);
			cprintf("+%d\n",eip-info.eip_fn_addr);
		}
		esp=ebp;
		ebp=*((uint32_t*)esp);
	}
	return 0;
}

static void
showmapping(uint32_t pa)
{
	pte_t* pg_table = pgdir_walk(kern_pgdir, (void *)pa, 0);
	if (pg_table == NULL || !(*pg_table & PTE_P)) 
		cprintf("The page doesn't exist!\n");
	else
	{
		cprintf("0x%08x - 0x%08x: ", PTE_ADDR(*pg_table), PTE_ADDR(*pg_table) + PGSIZE - 1);
		bool user = *pg_table & PTE_U, write = *pg_table & PTE_W;
		if (user) cprintf("user: ");
		else cprintf("kernel: ");
		if (write) cprintf("read/write.\n");
		else cprintf("read only.\n");
	}
}
int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 2)
	{
		uint32_t pa = strtol(argv[1], NULL, 0);
		showmapping(pa);
	}
	else if (argc == 3)
	{
		uint32_t st = strtol(argv[1], NULL, 0);
		uint32_t en = strtol(argv[2], NULL, 0);
		if (st <= en)
		{
			if (st & 0xfff) 
			{
				showmapping(st);
				st = (st & ~0xfff) + PGSIZE;
			}
			for(;st <= en;st += PGSIZE)
				showmapping(st);
		}
	}
	else 
	{
		cprintf("usage[1]: showmappings 0x1234\n");
		cprintf("usage[2]: showmappings 0x1234 0xabcd\n");
	}
	return 0;
}

static int getperm(char *s)
{
	int perm = -1;
	if (strlen(s) == 1 && s[0] == '0') perm = 0;
	else if (strlen(s) == 1 && s[0] == 'U') perm = PTE_U;
	else if (strlen(s) == 1 && s[0] == 'W') perm = PTE_W;
	else if (strlen(s) == 2 && s[0] == 'U' && s[1] == 'W')
		perm = PTE_U | PTE_W;
	return perm;
}

static void setperm(uint32_t pa, int perm)
{
	pte_t* pg_table = pgdir_walk(kern_pgdir, (void *)pa, 0);
	if (pg_table == NULL || !(*pg_table & PTE_P)) 
		cprintf("The page doesn't exist!\n");
	else
	{
		cprintf("Before: ");
		showmapping(pa);
		*pg_table &= ~PTE_U;
		*pg_table &= ~PTE_W;
		*pg_table |= perm;
		cprintf("After:  ");
		showmapping(pa);
	}
}

int
mon_setperms(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 3)
	{
		uint32_t pa = strtol(argv[1], NULL, 0);
		int perm = getperm(argv[2]);
		if (perm == -1) cprintf("unknown perm!\n");
		else setperm(pa, perm);
	}
	else if (argc == 4)
	{
		uint32_t st = strtol(argv[1], NULL, 0);
		uint32_t en = strtol(argv[2], NULL, 0);
		int perm = getperm(argv[3]);
		if (perm == -1) cprintf("unknown perm!\n");
		else 
		{
			if (st <= en)
			{
				if (st & 0xfff) 
				{
					setperm(st, perm);
					st = (st & ~0xfff) + PGSIZE;
				}
				for(;st <= en;st += PGSIZE)
					setperm(st, perm);
			}
		}
	}
	else 
	{
		cprintf("usage[1]: setperms address [0 | U | W | UW]\n");
		cprintf("usage[2]: setperms start_address end_address [0 | U | W | UW]\n");
	}
	return 0;
}

static void dump(uint32_t pa, int address_type)
{
	if (address_type) pa = (uint32_t) KADDR(pa);
	pte_t* pg_table = pgdir_walk(kern_pgdir, (void *)pa, 0);
	if (pg_table == NULL || !(*pg_table & PTE_P)) 
		cprintf("The page doesn't exist!\n");
	else
	{
		*pg_table &= ~PTE_P;
		cprintf("Dump:   0x%08x - 0x%08x\n", PTE_ADDR(*pg_table), PTE_ADDR(*pg_table) + PGSIZE - 1);
	}
} 

int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 3)
	{
		uint32_t pa = strtol(argv[2], NULL, 0);
		if (strlen(argv[1]) == 1 && argv[1][0] == 'v') dump(pa, 0);
		else if (strlen(argv[1]) == 1 && argv[1][0] == 'p') dump(pa, 1);
		else cprintf("unknown address type!\n");
	}
	else if (argc == 4)
	{
		uint32_t st = strtol(argv[2], NULL, 0);
		uint32_t en = strtol(argv[3], NULL, 0);
		int address_type = -1;
		if (strlen(argv[1]) == 1 && argv[1][0] == 'v') address_type = 0;
		else if (strlen(argv[1]) == 1 && argv[1][0] == 'p') address_type = 1;
		else cprintf("unknown address type!\n");
		if (address_type != -1)
		{
			if (st <= en)
			{
				if (st & 0xfff) 
				{
					dump(st, address_type);
					st = (st & ~0xfff) + PGSIZE;
				}
				for(;st <= en;st += PGSIZE)
					dump(st, address_type);
			}
		}
	}
	else 
	{
		cprintf("usage[1]: dump [v | p] 0x1234\n");
		cprintf("usage[2]: dump [v | p] 0x1234 0xabcd\n");
	}
	return 0;
}

static int getOutType(char *s)
{
	if (strlen(s) == 1)
	{
		if (s[0] == 'b') return 0;
		if (s[0] == 'B') return 1;
		if (s[0] == 'c') return 2;
		if (s[0] == 'd') return 3;
		if (s[0] == 'x') return 4;
	}
	return -1;
}

static void lookmem(uint32_t pa, int address_type, int o1, int o2)
{
	uint32_t tmp = pa;
	if (address_type) pa = (uint32_t) KADDR(pa);
	if (address_type) cprintf("At physical memory 0x%08x: ", tmp);
	else cprintf("At virtual memory 0x%08x: ", tmp); 
	uint32_t ans;
	if (o1 == 0) ans = *((unsigned char*) pa);
	else ans = *((uint32_t*) pa);
	if (o2 == 2) cprintf("%c\n", (char)ans);
	if (o2 == 3) cprintf("%d\n", ans);
	if (o2 == 4 && o1 == 0) cprintf("0x%02x\n", ans);
	if (o2 == 4 && o1 == 1) cprintf("0x%08x\n", ans);
} 

int
mon_lookmem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 5)
	{
		uint32_t pa = strtol(argv[2], NULL, 0);
		int o1 = getOutType(argv[3]), o2 = getOutType(argv[4]);
		if (o1 >= 0 && o1 <= 1 && o2 >= 2 && o2 <= 4)
		{
			if (strlen(argv[1]) == 1 && argv[1][0] == 'v') lookmem(pa, 0, o1, o2);
			else if (strlen(argv[1]) == 1 && argv[1][0] == 'p') lookmem(pa, 1, o1, o2);
			else cprintf("unknown address type!\n");
		}
		else cprintf("unknown output type!\n");
	}
	else if (argc == 6)
	{
		uint32_t st = strtol(argv[2], NULL, 0);
		uint32_t cnt = strtol(argv[3], NULL, 0);
		int address_type = -1;
		int o1 = getOutType(argv[4]), o2 = getOutType(argv[5]);
		if (strlen(argv[1]) == 1 && argv[1][0] == 'v') address_type = 0;
		else if (strlen(argv[1]) == 1 && argv[1][0] == 'p') address_type = 1;
		else cprintf("unknown address type!\n");
		if (o1 >= 0 && o1 <= 1 && o2 >= 2 && o2 <= 4)
		{
			if (address_type != -1)
			{
				for(int i = 0;i < cnt;i++)
					if (o1 == 0) lookmem(st + i, address_type, o1, o2);
					else lookmem(st + i * 4, address_type, o1, o2);
			}
		}
		else cprintf("unknown output type!\n");
	}
	else 
	{
		cprintf("usage[1]:lookmem [v | p] 0x1234 [b | B] [c | d | x]\n");
		cprintf("usage[2]:lookmem [v | p] 0x1234 cnt [b | B] [c | d | x]\n");
	}
	return 0;
}

int
mon_singlestep(int argc, char **argv, struct Trapframe *tf)
{
	if (argc > 1)
	{
		cprintf("usage:no extra arguments.\n");
	}
	else
	{
		if (!enable_single_step)
		{
			enable_single_step = true;
			uint32_t eflags = read_eflags();
			eflags |= FL_TF;
			//tf->tf_eflags |= FL_TF;
			cprintf("eflags: %x\n", eflags);
			cprintf("tf_eflags: %x\n", tf->tf_eflags);
			write_eflags(eflags);
		}
		return -1;
	}
	return 0;
}

int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	if (argc > 1)
	{
		cprintf("usage:no extra arguments.\n");
	}
	else
	{
		enable_single_step = false;
		tf->tf_eflags &= ~FL_TF;
		return -1;
	}
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;
	if (enable_single_step)
	{
		cprintf("eip: %x\n", tf->tf_eip);
		//enable_single_step = false;
	}
	else
	{
		cprintf("Welcome to the JOS kernel monitor!\n");
		cprintf("Type 'help' for a list of commands.\n");
	}

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		if (enable_single_step)
			buf = readline("D> ");
		else buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
