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
#include "checkpoint.h"

using namespace std;

void * checkpoint::find_stack_top(void){
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


	checkpoint::checkpoint(){
		failed = false;
		is_speculating = false;
		stack_start = (size_t) find_stack_top();
	}

 	bool checkpoint::checkpoint_begin(){
 		//find the size of the stack
 		char bottom_of_stack;
                xmemory::checkpoint();
 		size_t stack_size = stack_start - (size_t) &bottom_of_stack;
 		stack_size_at_checkpoint_begin = stack_size;

 		memcpy(stack_copy, (void*) (stack_start - stack_size), stack_size);

 		is_speculating = true;
                failed=false;
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

 		__asm__ __volatile__("movq %0, %%r13;": : "r" (this): "memory");
 		__asm__ __volatile__("movq %%rsp, %0;": "=r"(esp));

    	__asm__ __volatile__ ("label:");

		__asm__ __volatile__ ("movq 0x70(%%r13), %%r15;" : :);
 		__asm__ __volatile__ ("movq 0x68(%%r13), %%r14;" : :);

                bool _failed;
 		__asm__ __volatile__("movb 0x78(%%r13), %0;": "=r"(_failed));

                /*cout << "check_begin " << _failed << " r15test " << r15test << " r15 " << r15 << " r14 " << r14 << " pid " << getpid() << " "
                     << this << " &r15 " << &r15 << " &r14 " << &r14 << " r13 " << r13test << " " << *((unsigned long *)(((unsigned char *)this)+0x70)) <<
                     " " << ((unsigned long *)(((unsigned char *)this)+0x70)) << endl;*/

		return (_failed ? false : true);
 	}

 	void checkpoint::checkpoint_revert(){

 		// revert globals and heap
 		xmemory::revert_heap_and_globals();
 		__asm__ __volatile__("movq %0, %%r13;": : "r" (this) : "memory");

 		//failed = true
 		__asm__ __volatile__ ("movb $1, 0x78(%%r13);" : :);
 		//is_speculating = false
 		__asm__ __volatile__ ("movb $0, 0x6400090(%%r13);" : :);
		
		//restore registers
 		__asm__ __volatile__ ("movq 0x20(%%r13), %%rdx;" : :);
 		__asm__ __volatile__ ("movq 0x30(%%r13), %%r8;" : :);
 		__asm__ __volatile__ ("movq 0x38(%%r13), %%r9;" : : );

 		__asm__ __volatile__ ("movq 0x40(%%r13), %%rax;" : :);
 		__asm__ __volatile__ ("movq 0x48(%%r13), %%r10;" : :);
 		__asm__ __volatile__ ("movq 0x50(%%r13), %%r11;" : :);
 		__asm__ __volatile__ ("movq 0x58(%%r13), %%r12;" : :);
 		__asm__ __volatile__ ("movq 0(%%r13), %%r15;": : : "memory"); // ebp
 		__asm__ __volatile__("movq 0x8(%%r13), %%r14;": :); //esp

 		//memcpy
 		__asm__ volatile ( "cld\n\t" "rep movsb" : : "S" (stack_copy), "D" (stack_start - stack_size_at_checkpoint_begin), "c" (stack_size_at_checkpoint_begin) );

                __asm__ __volatile__ ("movq 0x10(%%r13), %%rdi;" : :);
 		__asm__ __volatile__ ("movq 0x18(%%r13), %%rsi;" : :);
 		__asm__ __volatile__ ("movq 0x28(%%r13), %%rcx;" : :);
                
 		__asm__ __volatile__ ("movq %r15, %rbp");
 		__asm__ __volatile__ ("movq %r14, %rsp");

 		//jmp to IP
 		__asm__ __volatile__ ("jmp label");
 	}

