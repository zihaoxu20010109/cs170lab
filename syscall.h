#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdlib.h>
#include "dllist.h"
#include "scheduler.h"
#include <errno.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void printDll();
void printJrb(JRB tree);
void test_fds();
void do_dup(void* arg);
void do_dup2(void* arg);
int get_next_fd(struct PCB* arg);
void do_write(void* arg);
void do_close(void* arg);
void do_fork(void* arg);
void do_getppid(void* arg);
void do_exit(void* arg);
void finish_fork(void* arg);
void do_wait(void* arg);
void get_pid(void* arg);
int ValidAddress(void *arg, int addr);
void getdtablesize(void *arg);
void sbreak(void* arg);
void getpagesize(void* arg);
void exec_ve(void* arg);
void fstat(void* arg);
void ioctl(void* arg);
void do_read(void* arg);
void syscall_return(void *arg, int value);
void initialize_sema();
void init_pipe(void* arg);

#endif