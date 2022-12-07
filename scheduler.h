#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdlib.h>
#include "dllist.h"
#include "kt.h"
#include "jrb.h"

extern kt_sem writeok;
extern kt_sem writers;
extern kt_sem readers;

extern kt_sem nelem;
extern kt_sem nslots;
extern kt_sem consoleWait;
struct PCB* init;

extern struct PCB* processes[8];

extern int currPID;
extern JRB pidTree;

//extern tells compiler that the variable is going to be defined somewhere, dont allocate memory, everytime h 
//file, copy paste into whatever it is using, prevents duplicate variables from being defined
struct PCB* currProcess; 
Dllist readyq;

void PrintStack(int sp, int mem_base);
void* initialize_user_process(void* args);
void scheduler();
int perform_execve(struct PCB* job, char* fn, char** argv);
int get_new_pid(); // returns unsused process ID
void destroy_pid(int pid);

typedef enum{Stin, Stout, Sterr, Invalid, Pipe_read, Pipe_write} Type;

struct Fd{
	bool isopen;
	struct Pipe* pipe;
	Type status;
	kt_sem* dupWait;
};

struct Pipe {
	// scheduling of processes accessing pipe is round robin, but reads and writes are atomic
	unsigned char* data;
	struct PCB* currWriter; // list of writers
	struct PCB* currReader; // list of readers
	
	int writeRef;
	int readRef;

	kt_sem* reader;
	kt_sem* writer;
	kt_sem* available; // # of available bytes in pipe
	kt_sem* used; // # of occupied bytes in pipe

	kt_sem* readerReady;

	int head; // index of head
	int tail; // index of tail
	int size;
	
};

struct PCB {
	int* registers;
	int sbrk;
	int base;
	int limit;
	int exitVal;
	unsigned short pid;
	struct PCB* parent;
	kt_sem waiters_sem;
	JRB children;
	Dllist waiters;
	//declare size of file table to be 64
	struct Fd *fd[64]; // fd identifier is its index in fd
};

#endif