/*
 * console_buf.c -- stub to handle user mode exceptions, including system calls
 * 
 * Everything else core dumps.
 * 
 * Copyright (c) 1992 The Regents of the University of California. All rights
 * reserved.  See copyright.h for copyright notice and limitation of
 * liability and disclaimer of warranty provisions.
 */
#include <stdlib.h>
#include "simulator.h"
#include "console_buf.h"
#include "kt.h"
#include "scheduler.h"

kt_sem nelem;
kt_sem nslots;
kt_sem consoleWait;
struct console_buf* buffer;

struct console_buf* init_buff() {
    struct console_buf* b;
    b = malloc(sizeof(struct console_buf));
    b->buff = malloc(256*sizeof(int));
    b->head = 0;
    b->tail = 0;
    b->size = 256;
    b->isEmpty = 1;
    return b;
}
// initialize console read buffer sems
void init_cons_sema() {
    nelem = make_kt_sem(0);
    nslots = make_kt_sem(256);
    consoleWait = make_kt_sem(0);
}

// getting console from 
void cons_to_buff(struct console_buf* buffer) {
    while (1) {
        P_kt_sem(consoleWait);
        char c = (char)console_read();
        // adding char to buffer head
        V_kt_sem(nelem);
        P_kt_sem(nslots);

        buffer -> buff[(buffer -> tail)%(buffer->size)] = c;
        if ((buffer -> tail + 1)%(buffer->size) != buffer -> head) {
            buffer -> tail = (buffer -> tail + 1)%(buffer->size);
        }
    }
}

