/*
 * kos.c -- starting point for student's os.
 * 
 */
#include "console_buf.h"
#include <stdlib.h>
#include "simulator.h"
#include "scheduler.h"
#include "kos.h"
#include "syscall.h"

int currPID;
JRB pidTree;
struct PCB* processes[8] = {NULL};

void KOS()
{
	initialize_sema();
	init_cons_sema();
	bzero(main_memory, MemorySize);

	// test_fds();

	currPID = 1;
	pidTree = make_jrb();

	kt_fork((void*)initialize_user_process, (void*)kos_argv);
	
	readyq = new_dllist();
	buffer = init_buff();

	printf("Running user code.\n");
	kt_fork((void*)(cons_to_buff), (void*)(buffer));

	scheduler();
}