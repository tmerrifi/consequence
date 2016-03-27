/*
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

/*
 * @file  xthread.cpp
 * @brief  Functions to manage thread related spawn, join.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#include <pthread.h>
#include <syscall.h>
#include "xthread.h"
#include "xatomic.h"
#include "xrun.h"
#include "debug.h"

unsigned int xthread::_nestingLevel = 0;
int xthread::_tid;

void * xthread::spawn(threadFunction * fn, void * arg, int parent_index, ThreadPoolEntry * tpe){
    ThreadStatus * t;
    if (tpe->thread_status_obj==NULL){
        // Allocate an object to hold the thread's return value.
        void * buf = allocateSharedObject(4096);
        HL::sassert<(4096 > sizeof(ThreadStatus))> checkSize;
        t = new (buf) ThreadStatus;

        ThreadPool::getInstance().set_threadstatus(tpe, (void *)t);
    }
    else{
        t=(ThreadStatus *)tpe->thread_status_obj;
    }

    t->threadIndex=tpe->id;
    
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << " got threadpool entry, arg: " << *((int *)arg) << endl;
#endif
    return forkSpawn(fn, arg, parent_index, tpe);
}

/// @brief Get thread index for this thread. 
int xthread::getThreadIndex(void * v) {
  assert(v != NULL);

  ThreadStatus * t = (ThreadStatus *) v;

  return t->threadIndex;
}

/// @brief Get thread tid for this thread. 
int xthread::getThreadPid(void * v) {
    assert(v != NULL);

    ThreadStatus * t = (ThreadStatus *) v;

    return t->tid;
}

/// @brief Do pthread_join.
void xthread::join(void * v, void ** result) {
  ThreadStatus * t = (ThreadStatus *) v;
  
  // Grab the thread result from the status structure (set by the thread),
  // reclaim the memory, and return that result.
  if (result != NULL) {
    *result = t->retval;
  }

  // Free the shared object held by this thread.
  //freeSharedObject(t, 4096);

  return;
}

/// @brief Cancel one thread. Send out a SIGKILL signal to that thread
int xthread::cancel(void *v) {
  ThreadStatus * t = (ThreadStatus *) v;

  int threadindex = t->threadIndex;

  //turn this on if we need to signal threads to get specstats
  //kill(t->tid, SIGUSR1);
  
  kill(t->tid, SIGKILL);

  // Free the shared object held by this thread.
  //freeSharedObject(t, 4096);
  return threadindex;
}

int xthread::thread_kill (void *v, int sig)
{
  int threadindex;
  ThreadStatus * t = (ThreadStatus *) v;

  threadindex = t->threadIndex;

  kill(t->tid, sig);
  
  //freeSharedObject(t, 4096);
  return threadindex;
}

void xthread::do_work(int parent_index, ThreadPoolEntry * tpe){
    while(true){
        ThreadStatus * t = (ThreadStatus *)tpe->thread_status_obj;
        pid_t mypid = syscall(SYS_getpid);
        setId(mypid);
        t->tid = mypid;
        //only do this if we're a new thread
        if (tpe->status==THREAD_POOL_ENTRY_INIT){
            determ_task_clock_init_with_id(tpe->id);
        }
        else{
            //if we don't do this, it thinks its already "waiting"
            //and will just keep spinning forever
            determ_task_clock_on_wakeup();
        }
        xrun::childRegister(mypid, parent_index, tpe->id, tpe->status==THREAD_POOL_ENTRY_INIT);
        _nestingLevel++;
        run_thread(tpe->run_function, t, tpe->run_function_arg);
        _nestingLevel--;
        //park the thread
        ThreadPool::getInstance().park_thread(tpe);
        if (tpe->status==THREAD_POOL_ENTRY_EXIT){
            break;
        }
    }
    xrun::finalThreadExit();
    _exit(0);
}

void * xthread::forkSpawn(threadFunction * fn, void * arg, int parent_index, ThreadPoolEntry * tpe) {

    if (tpe->status==THREAD_POOL_ENTRY_INIT){
        // Use fork to create the effect of a thread spawn.
        // FIXME:: For current process, we should close share.
        // children to use MAP_PRIVATE mapping. Or just let child to do that in the beginning.
        int child_id = syscall(SYS_clone, CLONE_FS | CLONE_FILES | SIGCHLD, (void*) 0);
        
        if (child_id==0){
            //we are the child process
            do_work(parent_index, tpe);
            return NULL;
        }
    }
    return (void *) tpe->thread_status_obj;
}

// @brief Execute the thread.
void xthread::run_thread(threadFunction * fn, ThreadStatus * t, void * arg) {
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "RUN THREAD, done initializing " << t->threadIndex << endl;
#endif
    determ_task_clock_start();
    void * result = fn(arg);
    xrun::threadDeregister();
    t->retval = result;
}
