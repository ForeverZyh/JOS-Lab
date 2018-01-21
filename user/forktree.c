// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

#define DEPTH 3

void forktree(const char *cur, int rand);

void
forkchild(const char *cur, char branch, int rand)
{
	char nxt[DEPTH+1];

	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	if (fork_priority(rand) == 0) {
	//if (fork() == 0) {
		forktree(nxt, (rand * 233 + 7)&0xff);
		exit();
	}
}

void
forktree(const char *cur, int rand)
{
	cprintf("%04x: I am '%s' with priority %d\n", sys_getenvid(), cur, sys_getenv_priority());

	forkchild(cur, '0', rand);
	forkchild(cur, '1', rand);
}

void
umain(int argc, char **argv)
{
	forktree("", 7);
}

