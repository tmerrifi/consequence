
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

	static inline void * find_stack_top(void){
		cout << "find stack" << endl;
		void	*start;
		void	*end;
		char i[4096];
		int n = 0;
		int ps;
		char* p;
		char var_on_stack;

		int fd = open("/proc/self/maps", O_RDONLY);
		
		while (read(fd, &i[n], sizeof(char))){

			if (i[n] == 10){

				int j = 0;
				while (i[j] != '-'){
					j++;
				}
		
				start = (void *)strtoul(i, NULL, 16);
				i[j] = '\0';
				char* ptr = i+j+1; 
				end = (void *)strtoul(ptr, NULL, 16);
		
				if ((void *)&var_on_stack >= start && (void *)&var_on_stack <= end) {
					close(fd);
					return end;
				}
				else{
					n = -1;
				}
 			}
 			n++;
		}
		
		close(fd);
		cout << "Error in finding top of the stack" << endl;
		return 0;
	}

 public:
 	bool is_speculating;

	checkpoint(){
		cout << "constructor" << endl;
		failed = false;
		is_speculating = false;
		stack_start = (size_t) find_stack_top();
	}

 	bool checkpoint_begin(){
 		cout << "begin" << endl;
 
 		if (xmemory::get_dirty_pages()){
 			cout << "dirty pages on heap or globals" << endl;
 			exit(-1);
 		}

 		//find the size of the stack
 		char bottom_of_stack;
 		size_t stack_size = stack_start - (size_t) &bottom_of_stack;
 		stack_size_at_checkpoint_begin = stack_size;

 		memcpy(stack_copy, (void*) (stack_start - stack_size), stack_size);

 		is_speculating = true;

 		//store registers
 		__asm__ __volatile__("movq %%rbp, %0;": "=r"(ebp));
 		__asm__ __volatile__("movq %%rdi, %0;": "=r"(rdi));
 		__asm__ __volatile__("movq %%rsi, %0;": "=r"(rsi));
 		__asm__ __volatile__("movq %%rdx, %0;": "=r"(rdx));
 		__asm__ __volatile__("movq %%rcx, %0;": "=r"(rcx));
 		__asm__ __volatile__("movq %%r8, %0;": "=r"(r8));
 		__asm__ __volatile__("movq %%r9, %0;": "=r"(r9));

 		__asm__ __volatile__("movq %%rax, %0;": "=r"(rax));
 		__asm__ __volatile__("movq %%r10, %0;": "=r"(r10));
 		__asm__ __volatile__("movq %%r11, %0;": "=r"(r11));
 		__asm__ __volatile__("movq %%r12, %0;": "=r"(r12));
 		__asm__ __volatile__("movq %%r13, %0;": "=r"(r13));
 		__asm__ __volatile__("movq %%r14, %0;": "=r"(r14));
 		__asm__ __volatile__("movq %%r15, %0;": "=r"(r15));

 		__asm__ __volatile__("movq %0, %%r13;": : "r" (this));
 		// __asm__ __volatile__("movq %%rsp, %0;": "=r"(esp));

    	__asm__ __volatile__ ("label:");

		__asm__ __volatile__ ("movq 0x48(%%r13), %%r15;" : :);
 		__asm__ __volatile__ ("movq 0x50(%%r13), %%r14;" : :);

    	bool _failed;
 		__asm__ __volatile__("movb 0x78(%%r13), %0;": "=r"(_failed));

    	// __asm__ __volatile__ ("movq %0, %%r15;" : : "r" (r15));

		return (_failed ? false : true);
 	}

 	void checkpoint_revert(){
 		cout << "revert" << endl;

 		// revert globals and heap
 		xmemory::revert_heap_and_globals();

 		xmemory::update();

 		//failed = true
 		__asm__ __volatile__ ("movq $1, 0x78(%%r13);" : :);
 		//is_speculating = false
 		__asm__ __volatile__ ("movq $0, 0x6400090(%%r13);" : :);
 		

 		__asm__ __volatile__("movq %0, %%r13;": : "r" (this));

		//restore registers
 		__asm__ __volatile__ ("movq 0x10(%%r13), %%rdi;" : :);
 		__asm__ __volatile__ ("movq 0x10(%%r13), %%rsi;" : :);
 		__asm__ __volatile__ ("movq 0x20(%%r13), %%rdx;" : :);
 		__asm__ __volatile__ ("movq 0x28(%%r13), %%rcx;" : :);
 		__asm__ __volatile__ ("movq 0x30(%%r13), %%r8;" : :);
 		__asm__ __volatile__ ("movq 0x30(%%r13), %%r9;" : : );

 		__asm__ __volatile__ ("movq 0x40(%%r13), %%rax;" : :);
 		__asm__ __volatile__ ("movq 0x48(%%r13), %%r10;" : :);
 		__asm__ __volatile__ ("movq 0x50(%%r13), %%r11;" : :);
 		__asm__ __volatile__ ("movq 0x58(%%r13), %%r12;" : :);
 		__asm__ __volatile__("movq 0(%%r13), %%r15;": :); // ebp
 		// __asm__ __volatile__("movq 0x8(%%r13), %%r14;": :); //esp

 		//memcpy
 		__asm__ volatile ( "cld\n\t" "rep movsb" : : "S" (stack_copy), "D" (stack_start - stack_size_at_checkpoint_begin), "c" (stack_size_at_checkpoint_begin) );

 		__asm__ __volatile__ ("movq %r15, %rbp");
 		// __asm__ __volatile__ ("movq %r14, %rsp");


 		//jmp to IP
 		__asm__ __volatile__ ("jmp label");
 	}
};

#endif

