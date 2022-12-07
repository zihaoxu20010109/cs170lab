#ifndef CONSOLE_BUF_H
#define CONSOLE_BUF_H

#include "kt.h"

struct console_buf* buffer;

struct console_buf {
    int* buff;
    int head;
    int tail;
    int isEmpty;
    int size;
};

struct console_buf* init_buff();
void init_cons_sema();
void cons_to_buff();

#endif