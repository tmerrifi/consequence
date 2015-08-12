
#ifndef _CHECKPOINT_H_
#define _CHECKPOINT_H_

#include "xdefines.h"
#include "xmemory.h"
#include <iostream>
#include <sys/mman.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fstream>
#include <sstream>

using namespace std;

#define MAX_STACK_SIZE xdefines::STACK_SIZE

class checkpoint{

private:
    uint64_t ebp;   //0x0
    uint64_t esp;   //0x8
    uint64_t rdi;   //0x10
    uint64_t rsi;   //0x18
    uint64_t rdx;   //0x20
    uint64_t rcx;   //0x28
    uint64_t r8;    //0x30
    uint64_t r9;    //0x38
    
    uint64_t rax;   //0x40
    uint64_t r10;   //0x48
    uint64_t r11;   //0x50
    uint64_t r12;   //0x58
    uint64_t r13;   //0x60
    uint64_t r14;   //0x68
    uint64_t r15;   //0x70
    
    bool failed;    //0x78
    size_t stack_start;
    unsigned char stack_copy[MAX_STACK_SIZE];
    size_t stack_size_at_checkpoint_begin;
    
    static void * find_stack_top(void);
    
 public:
    bool is_speculating;
    
    checkpoint();
    
    bool checkpoint_begin();
    
    void checkpoint_revert();
};

#endif

