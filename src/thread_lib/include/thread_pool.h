
#ifndef THREAD_POOL_H
#define THREAD_POOL_H


/*

  Copyright (c) 2012-15 Tim Merrifield, University of Illinois at Chicago


  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/


#define THREAD_POOL_MAX_SIZE 256

#define THREAD_POOL_MAX_STACK_SIZE (1<<14)

#define THREAD_POOL_ENTRY_EXIT 0 
#define THREAD_POOL_ENTRY_WAITING 1
#define THREAD_POOL_ENTRY_READY 2 
#define THREAD_POOL_ENTRY_INIT 3

#include <iostream>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <determ_clock.h>
#include <signal.h>
#include <setjmp.h>

#include "real.h"

//The thread-pool class is used to avoid costly thread startup times. It gives us
//a place to park threads after they finish running, and on fork we can grab a 
//thread from here.


typedef enum { STACK_ALLOCATED_ARG, HEAP_OR_GLOBAL_ARG } arg_allocation_t;

typedef struct _ThreadPoolEntry{
    void * (*run_function)(void *);
    void * run_function_arg;
    void * thread_status_obj;
    size_t id;
    uint32_t status;
    struct _ThreadPoolEntry * next, *prev;
    bool is_stack_allocated_var;
    uint8_t stack_space[THREAD_POOL_MAX_STACK_SIZE];
    size_t stack_size;
    uint8_t * stack_start_addr;
}ThreadPoolEntry;


//we use this to recover from segfaults when testing prior to copying the stack arg.
//Its only safe to make these static since its called while holding the token.
static jmp_buf __thread_pool_jmp_buf_env;

static void __thread_pool_segfault_handler(int sig_num){
    //jump passed the bad segfault instruction
    longjmp(__thread_pool_jmp_buf_env,1);
}

class ThreadPool{

 private:
    volatile int test_var;

    void save_parent_stack(ThreadPoolEntry * entry, void * arg, void * bottom_of_stack){
        struct sigaction seg_fault_action, old_action;
        entry->stack_size = 300;
        //intialize the sigaction
        memset(&seg_fault_action, 0, sizeof(struct sigaction));
        //set the signal handler function
        seg_fault_action.sa_handler=__thread_pool_segfault_handler;
        //clear the signal mask
        sigemptyset(&seg_fault_action.sa_mask);
        //grab the old action, if there was one
        sigaction (SIGSEGV, NULL, &old_action);
        //set the new action
        sigaction (SIGSEGV, &seg_fault_action, NULL);
        //initialize this address to null
        entry->stack_start_addr=NULL;
        if (setjmp(__thread_pool_jmp_buf_env)==0){
            //try and read from the arg passed in
            test_var=*((uint8_t *)arg);
            //if the previous line triggered a seg fault, we'll never get here
            memcpy(entry->stack_space, (uint8_t *)arg, entry->stack_size);
            entry->stack_start_addr=(uint8_t *)arg;
        }

        //put back the old sigaction
        if (old_action.sa_handler != SIG_IGN &&
                old_action.sa_handler != SIG_DFL){
                sigaction (SIGSEGV, &old_action, NULL);
        }
    }

    void copy_parent_stack(ThreadPoolEntry * entry){
        if (entry->stack_start_addr!=NULL && entry->stack_space!=NULL){
            memcpy(entry->stack_start_addr, entry->stack_space, entry->stack_size);
        }
    }

 public:
    
    void initialize(){
        //walk through the array and initialize each entry
        for (int i=0;i<THREAD_POOL_MAX_SIZE;i++){
            thread_arr[i].status=THREAD_POOL_ENTRY_INIT;
            //we use i+1 since 0 is used by the original thread of the program
            thread_arr[i].id=i+1;
            thread_arr[i].run_function_arg=NULL;
            thread_arr[i].run_function=NULL;
            thread_arr[i].thread_status_obj=NULL;
            thread_arr[i].is_stack_allocated_var=false;
            thread_arr[i].stack_size=0;
            thread_arr[i].stack_start_addr=NULL;
        }

        pthread_mutexattr_t attr;
        WRAP(pthread_mutexattr_init)(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        WRAP(pthread_mutex_init)(&lock, &attr);
        id_counter=0;
        head=tail=NULL;
        
    }

    static ThreadPool& getInstance(void) {
        static ThreadPool * threadPoolObject = NULL;
        if(!threadPoolObject) {
            void *buf = mmap(NULL, sizeof(ThreadPool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            threadPoolObject = new(buf) ThreadPool();
            threadPoolObject->initialize();
        }
        return * threadPoolObject;
    }

    ThreadPoolEntry * add_thread_to_pool_by_id(size_t id){
        assert(id>0);
        add_thread_to_pool(&thread_arr[id-1]);
    }

    ThreadPoolEntry * add_thread_to_pool(ThreadPoolEntry * entry){
        entry->next = entry->prev = NULL;
        WRAP(pthread_mutex_lock)(&lock);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "adding thread " << entry->id << " to pool" << endl;
#endif
        if (head==NULL){
            head=tail=entry;
        }
        else{
            entry->next=head;
            head->prev=entry;
            head=entry;
        }
        WRAP(pthread_mutex_unlock)(&lock);
        entry->is_stack_allocated_var=false;
#ifdef DISABLE_THREAD_POOL
        entry->status=THREAD_POOL_ENTRY_INIT;
#else
        entry->status=THREAD_POOL_ENTRY_WAITING;
#endif
    }

    void park_thread(ThreadPoolEntry * entry){
#ifdef DISABLE_THREAD_POOL
        entry->status=THREAD_POOL_ENTRY_EXIT;
#else
        syscall(SYS_futex, &entry->status, FUTEX_WAIT, THREAD_POOL_ENTRY_WAITING, NULL, NULL, 0);
        //after we wake up, if the argument is allocated on the stack...we have to copy the relevant stack data
        if (entry->is_stack_allocated_var){
            copy_parent_stack(entry);
        }
#endif
    }

    void set_threadstatus(ThreadPoolEntry * entry, void * thread_status_obj){
        entry->thread_status_obj=thread_status_obj;
    }

    void set_exit_by_id(size_t id){
        ThreadPoolEntry * entry = &thread_arr[id-1];
        entry->status=THREAD_POOL_ENTRY_EXIT;
        entry->is_stack_allocated_var=false;
    }

    //called while you hold the token
    ThreadPoolEntry * get_thread(void * (*run) (void *), void * arg, arg_allocation_t arg_alloc, void * bottom_of_stack){
        WRAP(pthread_mutex_lock)(&lock);
        ThreadPoolEntry * entry = tail;        
        if (entry){
            tail=entry->prev;
            if (entry->prev){
                entry->prev->next=NULL;
            }
            else{
                head=NULL;
            }
        }
        else if (id_counter < THREAD_POOL_MAX_SIZE - 1){
            entry=&thread_arr[id_counter];
            id_counter++;
            assert(entry->status==THREAD_POOL_ENTRY_INIT);
        }
            
        WRAP(pthread_mutex_unlock)(&lock);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "POOL: get_thread thread " << entry->id << " has been selected for creation " << endl;
#endif
        //set up the state
        entry->run_function=run;
        entry->run_function_arg=arg;
        entry->is_stack_allocated_var=(arg_alloc==STACK_ALLOCATED_ARG);
#ifndef DISABLE_THREAD_POOL
        if (entry->is_stack_allocated_var && entry->status!=THREAD_POOL_ENTRY_INIT){
            save_parent_stack(entry, arg, bottom_of_stack);
        }
#endif
        determ_task_clock_activate_other(entry->id);
        if (entry->status==THREAD_POOL_ENTRY_WAITING){
            entry->status=THREAD_POOL_ENTRY_READY;
            syscall(SYS_futex, &entry->status, FUTEX_WAKE, 1, NULL, NULL, 0);
        }
        else{
            //if its not waiting, set to INIT just to make sure we actually clone a real thread
            entry->status=THREAD_POOL_ENTRY_INIT;
        }
        return entry;
    }

    

    void terminate_all(){
#ifndef DISABLE_THREAD_POOL
        //all waiting threads need to exit
        for(int i=0;i<THREAD_POOL_MAX_SIZE;++i){
            if (thread_arr[i].status==THREAD_POOL_ENTRY_WAITING){
                thread_arr[i].status=THREAD_POOL_ENTRY_EXIT;
                syscall(SYS_futex, &thread_arr[i].status, 1, NULL, NULL, 0);
            }
        }
#endif
    }

 private:

    

    ThreadPoolEntry * head;
    ThreadPoolEntry * tail;
    pthread_mutex_t lock;
    ThreadPoolEntry thread_arr[THREAD_POOL_MAX_SIZE];
    uint32_t id_counter;
};

#endif
