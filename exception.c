/*
 * exception.c -- stub to handle user mode exceptions, including system calls
 * 
 * Everything else core dumps.
 * 
 * Copyright (c) 1992 The Regents of the University of California. All rights
 * reserved.  See copyright.h for copyright notice and limitation of
 * liability and disclaimer of warranty provisions.
 */
#include <stdlib.h>
#include "simulator.h"
#include "scheduler.h"
#include "kt.h"
#include "dllist.h"
#include "syscall.h"
#include "console_buf.h"

void
exceptionHandler(ExceptionType which)
{
	int             type, r5, r6, r7, newPC;
	int             buf[NumTotalRegs];

	examine_registers(buf);
	type = buf[4];
	r5 = buf[5]; //fd: name of the place to send the data, writing to the file sample, can be 0 or
	r6 = buf[6]; 
	r7 = buf[7];
	newPC = buf[NextPCReg];

	memcpy(currProcess->registers, buf, sizeof(buf));

	/*
	 * for system calls type is in r4, arg1 is in r5, arg2 is in r6, and
	 * arg3 is in r7 put result in r2 and don't forget to increment the
	 * pc before returning!
	 */

	switch (which) {
	case SyscallException:
		/* the numbers for system calls is in <sys/syscall.h> */
		switch (type) {
		case 0:
			/* 0 is our halt system call number */
			DEBUG('e', "Halt initiated by user program\n");
			SYSHalt();
		case SYS_exit:
			/* this is the _exit() system call */
			DEBUG('e', "_exit() system call\n");
			printf("Program exited with value %d.\n", r5);

			// DEBUG('e', (char*)ptr);
			int ptr = buffer->head;
			while (ptr != buffer -> tail) {
				printf("%c", (buffer->buff[ptr]));
				ptr = (ptr + 1)%(buffer->size);
			}
			printf("\n\n");
			kt_fork((void*)do_exit, (void*)currProcess);
			
			
			break;
		case SYS_write:
			kt_fork((void*)do_write, (void*)currProcess);
			DEBUG('e', "SYS_write() system call\n");
			break;
		case SYS_read:
			/* this is the read() system call */
			kt_fork((void*)do_read,(void *)currProcess);
			DEBUG('e', "SYS_read() system call\n");
			break;
		case SYS_ioctl:
			DEBUG('e', "SYS_ioctl() system call\n");
			kt_fork((void*)ioctl, (void *)currProcess);
			break;
		case SYS_fstat:
			kt_fork((void*)fstat, (void *)currProcess);
			DEBUG('e', "SYS_fstat() system call\n");
			break;
		case SYS_getpagesize:
			kt_fork((void*)getpagesize, (void *)currProcess);
			DEBUG('e', "getpagesize system call\n");
			break;
		case SYS_sbrk:
			kt_fork((void*)sbreak, (void *)currProcess);
			DEBUG('e', "sbrk system call\n");
			break;
		case SYS_execve:
			kt_fork((void*)exec_ve, (void *)currProcess);
			DEBUG('e', "execve system call\n");
			break;
		case SYS_getdtablesize:
			kt_fork((void*)getdtablesize, (void *)currProcess);
			DEBUG('e', "getdtablesize system call\n");
			break;
		case SYS_fork:
			kt_fork((void*)do_fork, (void *)currProcess);
			DEBUG('e', "fork system call\n");
			break;
		case SYS_getpid:
			kt_fork((void*)get_pid, (void *)currProcess);
			DEBUG('e', "getpid system call\n");
			break;
		case SYS_close:
			kt_fork((void*)do_close, (void *)currProcess);
			DEBUG('e', "close system call\n");
			break;
		case SYS_getppid:
			kt_fork((void*)do_getppid, (void *)currProcess);
			DEBUG('e', "getppid system call\n");
			break;
		case SYS_wait:
			kt_fork((void*)do_wait, (void *)currProcess);
			DEBUG('e', "wait system call\n");
			break;
		case SYS_dup:
			kt_fork((void*)do_dup, (void *)currProcess);
			DEBUG('e', "dup system call\n");
			break;
		case SYS_pipe:
			kt_fork((void*)init_pipe, (void *)currProcess);
			DEBUG('e', "pipe system call\n");
			break;
		case SYS_dup2:
			kt_fork((void*)do_dup2, (void *)currProcess);
			DEBUG('e', "dup2 system call\n");
			break;
		default:
			DEBUG('e', "Unknown system call\n");
			SYSHalt();
			break;
		}
		break;
	case PageFaultException:
		DEBUG('e', "Exception PageFaultException\n");
		break;
	case BusErrorException:
		DEBUG('e', "Exception BusErrorException\n");
		break;
	case AddressErrorException:
		DEBUG('e', "Exception AddressErrorException\n");
		break;
	case OverflowException:
		DEBUG('e', "Exception OverflowException\n");
		break;
	case IllegalInstrException:
		DEBUG('e', "Exception IllegalInstrException\n");
		break;
	default:
		printf("Unexpected user mode exception %d %d\n", which, type);
		exit(1);
	}
	scheduler();
}

void
interruptHandler(IntType which)
{
	if (currProcess != NULL) {
		int buf[NumTotalRegs]; 
		examine_registers(currProcess -> registers);
		dll_append(readyq, new_jval_v((void*)currProcess));
	}
	switch (which) {
	case ConsoleReadInt:
		DEBUG('e', "ConsoleReadInt interrupt\n");
		//V_kt_sem(readers);
		V_kt_sem(consoleWait); // ready to read from console!
		break;
	case ConsoleWriteInt:
		DEBUG('e', "ConsoleWriteInt interrupt\n");
		V_kt_sem(writeok);
		break;
	case TimerInt:
		// DEBUG('e', "Timer Interrupt\n");
		break;
	default:
		DEBUG('e', "Unknown interrupt\n");
		// noop();
		break;
	}
	scheduler();
}