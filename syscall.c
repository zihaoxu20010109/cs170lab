/*
 * syscall.c -- stub to handle user mode exceptions, including system calls
 * 
 * Everything else core dumps.
 * 
 * Copyright (c) 1992 The Regents of the University of California. All rights
 * reserved.  See copyright.h for copyright notice and limitation of
 * liability and disclaimer of warranty provisions.
 */
#include <stdlib.h>

#include "console_buf.h"
#include "simulator.h"
#include "scheduler.h"
#include "kt.h"
#include "dllist.h"
#include "syscall.h"
#include <errno.h>

kt_sem writeok;
kt_sem writers;
kt_sem readers;
kt_sem nelem;

void init_pipe(void *arg)
{
    //need to malloc every pipe data structure
    struct PCB *curr = (struct PCB *)arg;
    struct Pipe *pip = malloc(sizeof(struct Pipe));
    pip->tail = 1;
    pip->head = 1;
    pip->size = 8192;
    pip->data = malloc(sizeof(unsigned char) * 8192); // pipe can hold 8192 bytes of data

    int readFd = get_next_fd(curr);
    int writeFd = get_next_fd(curr);

    memcpy((curr->registers[5]) + main_memory + curr->base, &readFd, sizeof(4));
    memcpy((curr->registers[5]) + main_memory + curr->base + 4, &writeFd, sizeof(4));
    // int* fdArr = (int*)(curr -> registers[5]);

    curr->fd[readFd]->pipe = pip;
    curr->fd[writeFd]->pipe = pip;
    curr->fd[readFd]->status = Pipe_read;   // setting fd[0] to read end of pipe
    curr->fd[writeFd]->status = Pipe_write; // setting fd[1] to read end of pipe

    pip->reader = make_kt_sem(1); // atomic reads
    pip->writer = make_kt_sem(1); // atomic writes
    pip->available = make_kt_sem(pip->size);
    pip->used = make_kt_sem(0);

    pip->readerReady = make_kt_sem(0);

    pip->writeRef = 1; // ref counts
    pip->readRef = 1;

    syscall_return(curr, 0);
}

void do_dup(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    struct Pipe *currsWPipe = curr->fd[0]->pipe;
    struct Pipe *currsRPipe = curr->fd[1]->pipe;
    int prev_fd, newfd_index;
    prev_fd = curr->registers[5];
    newfd_index = get_next_fd(curr);
    if (newfd_index < 0)
    {
        syscall_return(curr, EBADF);
    }
    if (newfd_index > 64)
    {
        syscall_return(curr, EMFILE);
    }

    curr->fd[newfd_index] = curr->fd[prev_fd];

    if (curr->fd[newfd_index]->status == Pipe_write)
    {
        currsWPipe->writeRef++;
    }

    if (curr->fd[newfd_index]->status == Pipe_read)
    {
        currsRPipe->readRef++;
    }

    syscall_return(curr, newfd_index);
}

void do_dup2(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    struct Pipe *currsWPipe = curr->fd[curr->registers[5]]->pipe;
    struct Pipe *currsRPipe = curr->fd[curr->registers[5]]->pipe;
    int err;
    int prevfd = curr->registers[5];
    int newfd_index = curr->registers[6];

    if (prevfd < 0 || newfd_index < 0)
    {
        syscall_return(curr, EBADF);
    }
    if (prevfd > 64 || newfd_index > 64)
    {
        syscall_return(curr, EMFILE);
    }

    if (curr->fd[newfd_index]->isopen == TRUE)
    {
        curr->fd[newfd_index]->isopen = FALSE;
        curr->fd[newfd_index]->status = Invalid;
        if (curr->fd[newfd_index]->status == Pipe_write)
        {
            currsWPipe->writeRef--;
        }
        if (curr->fd[newfd_index]->status == Pipe_read)
        {
            currsRPipe->readRef--;
        }
        curr->fd[newfd_index]->pipe = NULL;
    }

    //is already has same value as old fd, do nothing, return new file descriptor

    if (curr->fd[newfd_index] == curr->fd[prevfd])
    {
        syscall_return(curr, newfd_index);
    }

    memcpy(curr->fd[newfd_index], curr->fd[prevfd], sizeof(struct Fd));

    if (curr->fd[newfd_index]->status == Pipe_write)
    {
        currsWPipe->writeRef++;
    }

    if (curr->fd[newfd_index]->status == Pipe_read)
    {
        currsRPipe->readRef++;
    }

    syscall_return(curr, newfd_index);
}

//gets next lowest fd available
int get_next_fd(struct PCB *curr)
{
    int i;
    for (i = 0; i < 64; i++)
    {

        if (!curr->fd[i]->isopen)
        {
            curr->fd[i]->isopen = TRUE;
            return i;
        }
    }
    return -1;
}

int ValidAddress(void *arg, int addr)
{
    struct PCB *curr = (struct PCB *)arg;
    if ((0 <= addr) && ((int)curr->limit >= addr))
    {
        DEBUG('e', "Entered into ValidAddress\n");
        return 1;
    }
    DEBUG('e', "Invalid Address\n");
    DEBUG('e', "base: %d\n", curr->base);
    DEBUG('e', "limit: %d\n", curr->limit);
    DEBUG('e', "addr: %d\n", addr);
    return 0;
}

void do_getppid(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    syscall_return(curr, curr->parent->pid);
}

void do_wait(void *arg)
{
    // SYSHalt();
    struct PCB *curr = (struct PCB *)arg;
    P_kt_sem(curr->waiters_sem); // issue with semaphore: process 1 is blocked by its own semaphore, never cleans up its zombies
    struct PCB *child = (struct PCB *)(jval_v((dll_val(dll_first(curr->waiters)))));
    dll_delete_node(dll_first(curr->waiters));
    int exit = child->exitVal;
    destroy_pid(child->pid);
    free(child->registers);
    free_dllist(child->waiters);
    jrb_free_tree(child->children);
    free(child);
    //return child->pid
    //store exit value in the place pointed to by r[5]

    syscall_return(curr, curr->pid);
}

void do_close(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    struct Pipe *currsWPipe;
    struct Pipe *currsRPipe;
    if (curr->registers[5] > 64)
    {
        syscall_return(curr, -EBADF);
    }
    if (curr->fd[0]->pipe)
    {
        currsWPipe = curr->fd[0]->pipe;
    }
    if (curr->fd[1]->pipe)
    {
        currsRPipe = curr->fd[1]->pipe;
    }
    int fd_index;
    fd_index = curr->registers[5];
    if (fd_index < 0)
    {
        syscall_return(curr, -EBADF);
    }

    curr->fd[fd_index]->isopen = FALSE;

    if (currsWPipe)
    {
        if (curr->fd[fd_index]->status == Pipe_write)
        {
            currsWPipe->writeRef--;
        }
    }

    if (currsRPipe)
    {
        if (curr->fd[fd_index]->status == Pipe_read)
        {
            currsRPipe->readRef--;
        }
    }

    syscall_return(curr, 0);
}

void do_fork(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    struct PCB *newproc;
    int hasFound = 0;
    for (int i = 0; i < 8; i++)
    {
        if (processes[i] == NULL)
        { // do we need to make sure the process technically fits in MemorySize/8??
            newproc = malloc(sizeof(struct PCB));
            memcpy(newproc, curr, sizeof(curr));
            newproc->registers = (int *)malloc(NumTotalRegs * sizeof(int));
            memcpy(newproc->registers, curr->registers, NumTotalRegs * sizeof(int));
            newproc->base = i * (MemorySize / 8);
            newproc->limit = MemorySize / 8; // -24 bytes at top, -1 since it's 0-indexed.
            memcpy(main_memory + newproc->base, main_memory + curr->base, curr->limit);

            for (int i = 0; i < 64; i++)
            {
                newproc->fd[i] = malloc(sizeof(struct Fd));
                // newproc -> fd[i] -> isopen = malloc(sizeof(bool));
                newproc->fd[i]->isopen = curr->fd[i]->isopen; // setting all file descriptors to same as parent
                newproc->fd[i]->status = curr->fd[i]->status;
                newproc->fd[i]->pipe = curr->fd[i]->pipe;
                //newproc -> fd[i] -> dupWait = curr -> fd[i] -> dupWait;
            }

            newproc->sbrk = curr->sbrk;

            newproc->parent = curr;
            newproc->waiters_sem = make_kt_sem(0);
            newproc->waiters = new_dllist();
            newproc->children = make_jrb();
            newproc->pid = get_new_pid();
            processes[i] = newproc;
            hasFound = 1;
            //inserting forked process into curr's child tree
            jrb_insert_int(curr->children, newproc->pid, new_jval_v((void *)newproc));
            break;
        }
    }
    if (hasFound == 0)
    {
        syscall_return(curr, EAGAIN);
    }
    else
    {
        kt_fork((void *)finish_fork, (void *)newproc);
        syscall_return(curr, newproc->pid);
    }
}

void finish_fork(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    syscall_return(curr, 0);
}

void do_exit(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    //release the memory
    for (int i = 0; i < 8; i++)
    {
        if (processes[i])
        {
            if (processes[i]->pid == curr->pid)
            {
                // release memory in main_memory
                // bzero(main_memory + processes[i] -> base, processes[i]->limit);
                processes[i] = NULL;
                break;
            }
        }
    }
    // if curr has a parent, delete itself from its parent's children tree.

    if (curr->parent)
    {
        jrb_delete_node(jrb_find_int(curr->parent->children, curr->pid));
    }

    // setting exit value
    curr->exitVal = curr->registers[5];
    // adding itself to its parent's waiters, or zombie list

    // move all of curr's children and make them init's children instead
    while (!jrb_empty(curr->children))
    {
        struct PCB *first = (struct PCB *)jval_v(jrb_val((jrb_first(curr->children))));
        jrb_delete_node(jrb_find_int(curr->children, first->pid));
        first->parent = init;
        jrb_insert_int(init->children, first->pid, new_jval_v((void *)first));
    }

    // release and free all of curr's zombie children
    while (!dll_empty(curr->waiters))
    {
        struct PCB *child = (struct PCB *)(dll_val((dll_first(curr->waiters))).v);
        child->parent = init;
        dll_append(init->waiters, dll_val(dll_first(curr->waiters)));
        dll_delete_node(dll_first(curr->waiters));
        V_kt_sem(init->waiters_sem);
    }

    Jval currVal = new_jval_v((void *)(curr));
    dll_append(curr->parent->waiters, currVal);
    // increment waiters_sem, or make parent closer to unblocking while waiting for its children to finish
    V_kt_sem(curr->parent->waiters_sem);

    // if curr is a child of init, delete and free itself.
    if (curr->parent->pid == init->pid)
    {
        destroy_pid(curr->pid);
        free_dllist(curr->waiters);
        jrb_free_tree(curr->children);
        free(curr->registers);
        free(curr);
    }
    //save exit value in PCB
    kt_exit();
}

void get_pid(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    syscall_return(curr, curr->pid);
}

void getdtablesize(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    syscall_return(curr, 64);
}

void exec_ve(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;

    char **argv = (char **)(curr->registers[6] + main_memory + User_Base);
    int numArgs = 0;
    while (argv[numArgs] != NULL)
    {
        numArgs++;
    }
    numArgs++;

    char **array = malloc(numArgs * sizeof(char *));
    // printf("curr -> registers[5]: %s\n", curr -> registers[5] + main_memory + User_Base);
    if (ValidAddress((void *)curr, (int)(curr->registers[6])) == 0)
    {
        syscall_return(curr, -1);
    }
    //mallocing array, need to strdup each section
    char *fn = strdup(curr->registers[5] + main_memory + User_Base); //returns pointer to string
                                                                     //mallocs memory for new string
                                                                     //copies new string into allocated space
                                                                     //increment r6 by 4 to get address of next pointer
                                                                     //insert into array (loop, strdp it)
                                                                     //do r6 + main_memory + 4

    int *offset; // gets the offset to access args in  argArr
    char *argArr;
    int j = 0;
    for (int i = 0; i < numArgs - 1; i++)
    {
        offset = (int *)((int)(curr->registers[6]) + User_Base + (int)main_memory + j);
        argArr = (char *)(*offset + main_memory + User_Base);
        array[i] = strdup(argArr);
        j += 4;
    }
    array[numArgs - 1] = '\0';
    int result = perform_execve(curr, fn, array);

    // curr -> registers[NextPCReg] = 0;

    free(fn);
    free(array);
    if (result == 0)
    {
        syscall_return(curr, 0);
    }

    syscall_return(curr, EINVAL);
}

void sbreak(void *arg)
{ //EXCEPTION HAPPENS HERE WHEN SYSCALL RETURNS A VALUE  OTHER THAN -1
    struct PCB *curr = (struct PCB *)arg;
    // syscall_return(curr, -1);
    int add = curr->registers[5];
    int oldsbrk = curr->sbrk;
    if (ValidAddress(curr, curr->sbrk + add) == 0)
    {
        syscall_return(curr, -1);
    }
    curr->sbrk += add;
    //return previous sbrk value
    syscall_return(curr, oldsbrk);
}

void fstat(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    int fd = curr->registers[5];

    if (ValidAddress((void *)curr, (curr->registers[6])) == 0)
    {
        syscall_return(curr, -1);
    }

    struct KOSstat *stat = (struct KOSstat *)(curr->registers[6] + main_memory + curr->base);

    if (fd == 0)
    {
        stat_buf_fill(stat, 1);
    }
    if (fd == 2 || fd == 1)
    {
        stat_buf_fill(stat, 256);
    }

    syscall_return(curr, 0);
}

void getpagesize(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    syscall_return(curr, PageSize);
}

//change to void
void ioctl(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    int fd = curr->registers[5];
    int params = curr->registers[6];

    if (ValidAddress((void *)curr, (int)(curr->registers[7])) == 0)
    {
        syscall_return(curr, -1);
    }

    struct JOStermios *addr = (struct JOStermios *)(curr->registers[7] + main_memory + curr->base);

    if (fd != 1 || params != JOS_TCGETP)
    {
        syscall_return(curr, EINVAL);
    }

    ioctl_console_fill(addr); // convert to user memory

    syscall_return(curr, 0);
}

void initialize_sema()
{
    writeok = make_kt_sem(1); //initialize outside in function (only need to fo it once, otherwise doing it each time called)
    writers = make_kt_sem(1); //might allow multiple to write
    readers = make_kt_sem(1);
}

//write end closes while reading, want to read up till amount in buffer
//read never returns 0

// ssize_t write(int fd, const void *buf, size_t count);
void do_write(void *arg)
{
    //if in, out, err do the exact same thing with console buff
    struct PCB *curr = (struct PCB *)arg;

    if (curr->fd[curr->registers[5]]->isopen == 0)
    {
        syscall_return(curr, -EBADF); // file descriptor is not open
    }

    int writeCount = 0;

    //address of something in user memory, needs to be between 0 and memorysize
    if ((curr->registers[6] < 0) || (curr->registers[6] >= (MemorySize / 8)))
    {
        syscall_return(curr, -EFAULT);
    }

    //make sure its nonnegative, make sure its in bounds count + buff
    if (curr->registers[7] < 0)
    {
        syscall_return(curr, -EINVAL);
    }

    if (curr->fd[curr->registers[5]]->status == Pipe_write)
    { //  writing to pipe
        // USE REF COUNT TO CHECK IF PIPE HAS READERS
        if (curr->fd[curr->registers[5]]->pipe->readRef == 0)
        {
            syscall_return(curr, -EPIPE);
        }
        P_kt_sem(curr->fd[curr->registers[5]]->pipe->writer); // Writer has pipe. Any other writer will block if trying to write at same time
        curr->fd[curr->registers[5]]->pipe->currWriter = curr;
        curr->registers[6] = (int)(curr->registers[6] + main_memory + curr->base); // converting to system address
        unsigned char *chars = (unsigned char *)(curr->registers[6]);
        int count = curr->registers[7];
        int totalSize = sizeof(chars);

        for (writeCount = 0; writeCount < count;)
        {
            struct Pipe *currsPipe = curr->fd[curr->registers[5]]->pipe;
            if (kt_getval(currsPipe->available) - 1 < 0)
            {
                V_kt_sem(currsPipe->writer); // releasing lock on pipe -- other pipe can interleave
                V_kt_sem(curr->fd[curr->registers[5]]->pipe->readerReady);
                P_kt_sem(currsPipe->writer); // attempting to reobtain lock
            }
            // taking available byte from pipe
            P_kt_sem(currsPipe->available);

            // similar to appending char to console buffer
            currsPipe->data[(currsPipe->tail) % (currsPipe->size)] = chars[writeCount];
            // printf("%c", currsPipe -> data[(currsPipe-> tail) % (currsPipe->size)]);
            if ((currsPipe->tail + 1) % (currsPipe->size) != currsPipe->head)
            {
                currsPipe->tail = (currsPipe->tail + 1) % (currsPipe->size);
            }
            writeCount++;
            // incrementing number of filled bytes in pipe
            V_kt_sem(curr->fd[curr->registers[5]]->pipe->readerReady);
            V_kt_sem(currsPipe->used);
        }
        V_kt_sem(curr->fd[curr->registers[5]]->pipe->writer);
    }
    else if (curr->fd[curr->registers[5]]->status == Stout || curr->fd[curr->registers[5]]->status == Sterr)
    { // writing to console (stdin)
        P_kt_sem(writers);
        curr->registers[6] = (int)(curr->registers[6] + main_memory + curr->base); // converting to system address
        char *chars = (char *)(curr->registers[6]);
        int count = curr->registers[7];
        int totalSize = sizeof(chars);
        //want to write until hit register 7
        for (char *i = chars; *i != '\0'; i++)
        {
            P_kt_sem(writeok);
            console_write(*i);
            writeCount++; //if writeCount = curr -> registers[7] - 1
            //while writecount < count, keep writing
        }
        V_kt_sem(writers);
    }
    else
    {
        syscall_return(curr, -EBADF); // file descriptor is neither writing to a pipe or writing to console
    }
    syscall_return(curr, writeCount);
}

// ssize_t read(int fd, void *buf, size_t count);
void do_read(void *arg)
{
    struct PCB *curr = (struct PCB *)arg;
    int count = 0;
    int numElems = curr->registers[7];
    if (curr->fd[curr->registers[5]]->isopen == 0)
    {
        syscall_return(curr, -EBADF);
    }

    if (((int)(curr->registers[6]) < 0) || ((int)(curr->registers[6]) >= (MemorySize / 8)))
    {
        syscall_return(arg, -EFAULT);
    }

    if (curr->registers[7] < 0)
    {
        syscall_return(arg, -EINVAL);
    }
    int hasClosed = 0;
    // from linux manual: ssize_t read(int fd, void *buf, size_t count);s
    if (curr->fd[curr->registers[5]]->status == Pipe_read)
    { // reading from pipe
        struct Pipe *currsPipe = curr->fd[curr->registers[5]]->pipe;
        currsPipe->currReader = curr;
        int numBytes = curr->registers[7];

        P_kt_sem(currsPipe->readerReady);
        P_kt_sem(currsPipe->reader); // obtaining pipe read lock
        if (kt_getval(currsPipe->used) == 0) {
            V_kt_sem(currsPipe->reader);
            syscall_return(curr, 0);
        }
        for (int i = 0; i < numBytes; i++)
        {
            if (currsPipe->writeRef == 0)
            { // has read all of buffer & no more writers
                V_kt_sem(currsPipe->reader);
                syscall_return(curr, 0);
            }
            if (kt_getval(currsPipe->used) == 0) {
                V_kt_sem(currsPipe->reader);
                syscall_return(curr, count);
            }
            P_kt_sem(currsPipe->used);
            char toRead = (char)(currsPipe->data[currsPipe->head]);
            ((char *)(curr->registers[6] + main_memory + curr->base))[i] = toRead;
            currsPipe->head = (currsPipe->head + 1) % (currsPipe->size);
            count++;
            V_kt_sem(currsPipe->available);
        }
        V_kt_sem(currsPipe->reader); // releasing pipe read lock
        syscall_return(curr, count);
    }
    else if (curr->fd[curr->registers[5]]->status == Stin)
    { // reading from console buff
        P_kt_sem(readers);
        int min = MIN(curr->registers[7], buffer->size);
        for (int i = 0; i < min; i++)
        {
            // reading from buffer, then removing and freeing char from buffer
            P_kt_sem(nelem);
            char toRead = (char)(buffer->buff[buffer->head]);
            if (toRead == EOF)
            {
                syscall_return(curr, count);
            }

            ((char *)(curr->registers[6] + main_memory + curr->base))[i] = toRead;

            buffer->head = (buffer->head + 1) % (buffer->size);
            V_kt_sem(nslots); // freeing a slot in the console buffer
            count++;
        }
        V_kt_sem(readers);
    }
    else
    {
        syscall_return(curr, -EBADF); // invalid read file descriptor
    }

    syscall_return(curr, count);
}

void syscall_return(void *arg, int value)
{
    //set PCReg in saved registers to NextPCReg
    struct PCB *toAdd = (struct PCB *)arg;
    toAdd->registers[PCReg] = toAdd->registers[NextPCReg];
    toAdd->registers[NextPCReg] = toAdd->registers[NextPCReg] + 4;
    //put return value into register 2
    toAdd->registers[2] = value;
    //put PCB onto ready queue
    dll_append(readyq, new_jval_v((void *)toAdd));
    //call kt_exit
    kt_exit();
}