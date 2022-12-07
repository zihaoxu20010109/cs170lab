/*
 * scheduler.c -- stub to handle user mode exceptions, including system calls
 * 
 * Everything else core dumps.
 * 
 * Copyright (c) 1992 The Regents of the University of California. All rights
 * reserved.  See copyright.h for copyright notice and limitation of
 * liability and disclaimer of warranty provisions.
 */

#include <stdlib.h>
#include "simulator.h"
#include <errno.h>
#include "scheduler.h"
#include "kt.h"
#include "dllist.h"

void PrintStack(int sp, int mem_base)
{
        int argc;
        int addr;
        int i;
        int str_at;
        /*
         * print three words of zeros at stack top
         */
        printf("sp:\t%8d: | %8d |\n",sp,*(int* )(&main_memory[sp+mem_base]));
        printf("\t%8d: | %8d |\n",sp+4,*(int *)(&main_memory[sp+4+mem_base]));
        printf("\t%8d: | %8d |\n",sp+8,*(int *)(&main_memory[sp+8+mem_base]));
        argc=*(int *)(&main_memory[sp+12+mem_base]);
        printf("\t%8d: | %8d | argc\n",
                sp+12,*(int *)(&main_memory[sp+12+mem_base]));
        printf("\t%8d: | %8d | &argv[0]\n",
                sp+16,*(int *)(&main_memory[sp+16+mem_base]));
        printf("\t%8d: | %8d | &envp[0]\n",
                sp+20,*(int *)(&main_memory[sp+20+mem_base]));
        for(i=0; i < argc; i++) {
                addr = sp+24+(i*4)+mem_base;
                str_at=*(int *)(&main_memory[addr]);
                printf("\t%8d: | %8d | argv[%d] -> %s\n",
                        sp+24+(i*4),*(int *)(&main_memory[addr]),
                        i,(char *)&main_memory[str_at+mem_base]);
        }
}

int get_new_pid() {
	int hasFound = 0;
	while (hasFound == 0) {
		if (jrb_find_int(pidTree,currPID) == NULL) { // if value returned is not equal to value being searched for
			jrb_insert_int(pidTree, currPID, new_jval_i(0));
			hasFound = 1;
		} else {
			currPID++;
		}
	}
	return currPID;
}

void destroy_pid(int pid) {
	JRB toDelete = jrb_find_int(pidTree,pid);
	if (toDelete != NULL) {
		jrb_delete_node(toDelete);
		if (pid < currPID) {
			currPID = pid;
		}
	}
}

void* initialize_user_process(void* args) {

	init = malloc(sizeof(struct PCB));
	init -> pid = 0;
	init -> waiters_sem = make_kt_sem(0);
	init -> waiters = new_dllist();
	init -> children = make_jrb();

	// allocating PCB
	struct PCB* proc = malloc(sizeof(struct PCB));
	proc -> registers = (int *)malloc(NumTotalRegs * sizeof(int));
	proc -> children = make_jrb();

	int isPlaced = 0;
	for (int i = 0; i < 8; i++) {
		if (processes[i] == NULL) {
			// initializing base and limit members
			proc -> base = (MemorySize/8)*i;
			proc -> limit = MemorySize/8;
			proc -> pid = get_new_pid();
			proc -> parent = init;
			proc -> waiters_sem = make_kt_sem(0);
			proc -> waiters = new_dllist();

			//sets all values to 0 (aka unused)
			for (int i = 0; i < 64; i++) {
				proc -> fd[i] = malloc(sizeof(struct Fd));
			}

			//first three args are already used
			proc -> fd[0] -> isopen = TRUE;
			proc -> fd[0] -> status = Stin;
			//proc -> fd[0] -> dupWait = make_kt_sem(1);

			proc -> fd[1] -> isopen = TRUE;
			proc -> fd[1] -> status = Stout;
			//proc -> fd[1] -> dupWait = make_kt_sem(1);

			proc -> fd[2] -> isopen = TRUE;
			proc -> fd[2] -> status = Sterr;
			//proc -> fd[2] -> dupWait = make_kt_sem(1);

			for (int i = 3; i < 64; i++) {
				proc -> fd[i] -> status = Invalid; // setting all file descriptor flags to invalid
				proc -> fd[i] -> isopen = FALSE;
				proc -> fd[i] -> pipe = NULL;
				//proc -> fd[i] -> dupWait = make_kt_sem(1);
			}

			proc -> sbrk = 0;
			processes[i] = proc;
			jrb_insert_int(init -> children, proc -> pid, new_jval_v((void*)proc));
			isPlaced = 1;
			break;
		}
	}
	if (isPlaced == 1) {
		// call to execve
		char** argstr = (char**)args;
		if(perform_execve(proc, argstr[0], argstr) == 0) {
			proc -> registers[PCReg] = 0;
			proc -> registers[NextPCReg] = 4;
			dll_append(readyq, new_jval_v((void*)proc));
		}
	}
	
	kt_exit();
}

int perform_execve(struct PCB* job, char* fn, char** argv) {
	int tos, k, arg_v;
	int numArgs = 0;
	while (argv[numArgs] != NULL) {
		numArgs++;
	}
	int ptr[numArgs];

	User_Base = job -> base;
	User_Limit = job -> limit;

	int result = load_user_program(argv[0]);
	if (result < 0) {
		fprintf(stderr,"Can't load program.\n");
		return -EFBIG;
	} else {
		job -> sbrk = result;
	}

	for (int i=0; i < NumTotalRegs; i++)
		job -> registers[i] = 0;


	job -> registers[PCReg] = 0;
	job -> registers[NextPCReg] = 0;

	tos = User_Limit - 24;

	int j = 0;
	for(int i = numArgs - 1; i >= 0; i--) {
		tos -= (strlen(argv[i])+1);
		strcpy(main_memory + User_Base + tos, argv[i]); //put strings in stack
		ptr[j] = tos;
		j++;
	}

	while (tos % 4) {
		tos--;
	}

	tos -= 4;
	k = 0;

	memcpy(main_memory+tos + User_Base, &k, 4);

	for(int i = 0; i < numArgs; i++) {
		tos -= 4;
		memcpy(main_memory+tos + User_Base, &ptr[i], 4);
	}
	arg_v = tos;

	tos -= 4;
    k = 0;
    memcpy(main_memory+tos + job -> base, &k, 4);
  
    tos -= 4;
    memcpy(main_memory+tos + job -> base, &arg_v, 4);
    tos -= 4;
    k = numArgs;
    memcpy(main_memory+tos + job -> base, &k, 4);

    job -> registers[StackReg] = tos - 12;

	return 0;
}

void scheduler() {
	kt_joinall();
	if (dll_empty(readyq) == 1) {
		currProcess = NULL;
		if (jrb_empty(init->children)) {
			SYSHalt();
		}
		noop();
	} else {
		struct PCB* top = (struct PCB*)((dll_val(dll_first(readyq))).v);
		currProcess = top;
		User_Base = currProcess -> base;
		User_Limit = currProcess -> limit;
		dll_delete_node(dll_first(readyq));
		start_timer(10);
		run_user_code(currProcess->registers);
	}
}