
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
	uint64_t ebp;
	uint64_t esp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t r8;
	uint64_t r9;

	uint64_t rax;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	bool failed;
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

