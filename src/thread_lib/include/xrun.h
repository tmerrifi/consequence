// -*- C++ -*-

#ifndef _XRUN_H_
#define _XRUN_H_

/*

  Copyright (c) 2007-8 Emery Berger, University of Massachusetts Amherst.
  
  Copyright (c) 2012-15 Tim Merrifield, University of Illinois at Chicago

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a coƒpy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/




// Common defines
#include "xdefines.h"
// threads
#include "xthread.h"
// memory
#include "xmemory.h"

#include "checkpoint.h"

// Heap Layers
#include "heaplayers/util/sassert.h"
#include "xatomic.h"
// determinstic controls
#include "determ.h"
#include "xbitmap.h"
#include "prof.h"
#include "debug.h"
#include "time_util.h"
#include "stats.h"
#include "logical_clock.h"
#include "thread_pool.h"
#include "conseq_malloc.h"
#include <sys/resource.h>
#include <determ_clock.h>
#include <signal.h>

#define MAX_SLEEP_COUNT 10

class xrun {


private:
    static volatile bool _initialized;
    static size_t _master_thread_id;
    static size_t _thread_index;
    static size_t _lock_count;
    static bool _token_holding;

    /******these variables keep track of some of the coarsening thread-local state*/
    static int tx_coarsening_counter;
    static int tx_current_coarsening_level;
    static int tx_consecutively_coarsened;
    static bool tx_monitor_next;
    /************************************************/

    /********Sleeping is done to make things easier on the Conversion garbage collector. 
    The GC is not concurrent, and does not collect versions that are newer than the oldest
    version currently held. In the event that the main thread creates some children, and then
    joins on them...and thus holding on to an early version...then no versions are ever 
    collected! Instead, we put the thread to "sleep" and that allows the GC to collect newer
    versions. The sleep_count is used to ensure we don't sleep/unsleep too much. We really don't
    want to do it too much as it adds some overhead.****/

    static int sleep_count;
    static bool is_sleeping;

    static uint64_t heapVersionToWaitFor;
    static uint64_t globalsVersionToWaitFor;
    
public:

  /// @brief Initialize the system.
  static void initialize(void) {

    Checkpoint* _checkpoint = new Checkpoint();
    cout << "HUH???" << endl;
    _initialized = false;
    _lock_count = 0;
    _token_holding = false;
    sleep_count=0;
    is_sleeping=false;
    tx_coarsening_counter=0;
    tx_consecutively_coarsened=0;
    heapVersionToWaitFor=0;
    globalsVersionToWaitFor=0;

    tx_current_coarsening_level=LOGICAL_CLOCK_MIN_ALLOWABLE_TX_SIZE;
    tx_monitor_next=false;
    installSignalHandler();

    pid_t pid = syscall(SYS_getpid);

    /* Get the current stack limit*/
    struct rlimit stack_limits;
    if (getrlimit(RLIMIT_STACK, &stack_limits)!=0){
        fprintf(stderr, "Getting stack size failed");
        ::abort();
    }

    /*set the stack size to be the defined stack size*/
    if (stack_limits.rlim_cur > xdefines::STACK_SIZE && (stack_limits.rlim_max==RLIM_INFINITY || 
                                                         stack_limits.rlim_max > xdefines::STACK_SIZE)){
        stack_limits.rlim_cur=xdefines::STACK_SIZE;
        //set max stack size
        if (setrlimit(RLIMIT_STACK, &stack_limits)!=0){
            fprintf(stderr, "Setting stack size failed");
            ::abort();
        }
    }
    
    if (!_initialized) {
      _initialized = true;
      xmemory::initialize();
      xthread::setId(pid);
      _master_thread_id = pid;
      xmemory::setThreadIndex(0);
      determ::getInstance().initialize();
      xbitmap::getInstance().initialize();
      _thread_index = 0;
      determ::getInstance().registerMaster(_thread_index, pid);
    } else {
      fprintf(stderr, "xrun reinitialized");
      ::abort();
    }
    determ_task_clock_activate();
    //start this thread's clock
    startClock();
  }

    static bool singleActiveThread(void){
#ifdef SINGLE_THREAD_OPT        
        return (determ_task_clock_single_active_thread() && (_thread_index==determ::getInstance().getLastTokenHolder()));
#else
        return false;
#endif
    }

    static inline bool inCoarsenedTx(){
#ifdef USE_USERSPACE_READING
        return (tx_coarsening_counter > 0);
#else
        return false;
#endif
    }

    static inline bool useTxCoarsening(size_t id){
#ifdef USE_TX_COARSENING
        int64_t next_tx=determ_task_clock_estimate_next_tx(id);
        if (next_tx >= 0 && (tx_coarsening_counter+next_tx) < tx_current_coarsening_level){
            if (tx_consecutively_coarsened==0){
                determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_START_COARSE, (void *)id);
            }
            tx_coarsening_counter+=determ_task_clock_get_last_tx_size();
            ++tx_consecutively_coarsened;
            return true;
        }
        else{
            //if we are a single thread, then we need to set the coarsening counter to 0 right here, since we won't mess with the token later
            if (singleActiveThread() && tx_coarsening_counter > 0){
                endTXCoarsening();
            }
            //if tx_coarsening_counter is greater than 0, then we are inside a coarsened tx. IF thats the case then lets monitor what happens next. If we are the
            //next thread to grab the token, then lets bump up the allowable tx size by some amount
            else if (tx_coarsening_counter>0){
                tx_monitor_next=true;

            }
            return false;
        }
#else
        return false;

#endif        
    }

    
    static inline void endTXCoarsening(){
#ifdef USE_TX_COARSENING
        if (inCoarsenedTx()){
            determ_task_clock_end_coarsened_tx();
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_END_COARSE, 0);
        }
        tx_coarsening_counter=0;
        tx_consecutively_coarsened=0;
#endif
    }

    static inline void resetTXCoarsening(){
#ifdef USE_TX_COARSENING
        tx_monitor_next=false;
        tx_current_coarsening_level=LOGICAL_CLOCK_MIN_ALLOWABLE_TX_SIZE;
        endTXCoarsening();
#endif
    }


  static void done(void){
      waitToken();
      determ::getInstance().finalize();
      ThreadPool::getInstance().terminate_all();
      putToken();
  }

  static void finalize(void) {
    xmemory::finalize();
  }

  // @ Return the main thread's id.
  static inline bool isMaster(void) {
    return getpid() == _master_thread_id;
  }

  // @return the "thread" id.
  static inline int id(void) {
    return xthread::getId();
  }


  // New created thread should call this.
  // Now only the current thread is active.
    static inline int childRegister(int pid, int parentindex, int child_index) {
    int threads;
    struct timespec t1,t2;


    clock_gettime(CLOCK_MONOTONIC, &t1);
    // Get the global thread index for this thread, which will be used internally.
    //_thread_index = xatomic::increment_and_return(&global_data->thread_index);
    _thread_index = child_index;
    _lock_count = 0;
    _token_holding = false;

    xmemory::wake();
#ifdef USE_TAGGING
        xmemory::set_local_version_tag(0xDEAD);
#endif
    determ::getInstance().registerThread(_thread_index, pid, parentindex);
    // Set correponding heap index.
    xmemory::setThreadIndex(_thread_index);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    waitToken();
    #ifdef TOKEN_ORDER_ROUND_ROBIN
        determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
    #endif
    commitAndUpdateMemory();
    putToken();
    determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
    return (_thread_index);
  }

  static inline void threadDeregister(void) {
      stopClock();
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
#ifdef DTHREADS_TASKCLOCK_DEBUG
      cout << "thread deregister " << determ_task_get_id() << " count " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif
      waitToken();
      commitAndUpdateMemory();
      if (determ::getInstance().is_master_thread_finisehd()){
          ThreadPool::getInstance().set_exit_by_id(_thread_index);
      }
      else{
          ThreadPool::getInstance().add_thread_to_pool_by_id(_thread_index);
      }

      xmemory::sleep();
      //the token is released in here....
      determ::getInstance().deregisterThread(_thread_index);
  }

    static inline void finalThreadExit(void){
        determ::getInstance().print_all_thread_events(_thread_index);
        determ_task_clock_close();
    }

  /// @return the unique thread index.
  static inline int threadindex(void) {
    return _thread_index;
  }

    //computing stack address by process of elimination
    static inline bool is_stack_addr(void * addr, void * end_of_stack){
        return (!(xmemory::inHeapRange(addr) || xmemory::inGlobalsRange(addr)) 
                && ( (size_t)addr > (size_t)end_of_stack && ((size_t)addr-(size_t)end_of_stack) < xdefines::STACK_SIZE ));
    }

  /// @brief Spawn a thread.
    static inline void * spawn(threadFunction * fn, void * arg, pthread_t * tid) {
        //we use this to designate the "end" of the user's stack. This i
        uint8_t end_of_user_stack_marker;

        stopClockNoCoarsen();

        if (_lock_count>0){
            cout << "FORKING WHILE HOLDING A LOCK...not currently supported" << endl;
            exit(-1);
        }
        
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);

        //now, lets initialize the thread so that there is not a race with it to get the token
        waitToken();
#ifdef PRINT_SCHEDULE
        cout << "SCHED: CREATING THREAD - tid: " << _thread_index << endl;
        fflush(stdout);
#endif
        //commit our memory so the spawned thread sees it
        commitAndUpdateMemory();
        //add the estimated time its going to take to fork a new process. 
        determ_task_clock_add_ticks(LOGICAL_CLOCK_TIME_FORK_PER_PAGE * xmemory::get_logical_pages());
        //get the thread pool entry. Need to do this while holding the token since we will wake up another thread
        ThreadPoolEntry * tpe = ThreadPool::getInstance().get_thread(fn, arg, (is_stack_addr(arg, &end_of_user_stack_marker) ? STACK_ALLOCATED_ARG : HEAP_OR_GLOBAL_ARG), 
                                                                               (void *)&end_of_user_stack_marker);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_FORKED_THREAD);
#endif
        //now release the token so other threads can keep on chuggin'
        putTokenNoFastForward();
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_FORK, NULL);
        *tid = (pthread_t)xthread::spawn(fn, arg, _thread_index, tpe);
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_FORK);

        waitToken();
        //commit memory in case the tid is allocated on the heap.
        commitAndUpdateMemory();
#ifdef NO_DETERM_SYNC
        determ::getInstance().starting_fork(tpe->id);
#endif
        putToken();
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
        startClock();
        return NULL;
    }
    
  /// @brief Wait for a thread.
  static inline void join(void * v, void ** result) {
    int  child_threadindex = 0;
    bool wakeupChildren = false;

    stopClockNoCoarsen();

    // Return immediately if the thread argument is NULL.
    if (v == NULL) {
      fprintf(stderr, "%d: join with invalid parameter\n", getpid());
      return;
    }

    // Wait on token if the fence is already started.
    // It is important to maitain the determinism by waiting. 
    // No need to wait when fence is not started since join is the first
    // synchronization after spawning, other thread should wait for 
    // the notification from me.
    waitToken();
    
    commitAndUpdateMemory();

    // Get the joinee's thread index.
    child_threadindex = xthread::getThreadIndex(v);

    xmemory::sleep();

#ifdef PRINT_SCHEDULE
    cout << "SCHED: BEGIN JOIN - tid: " << _thread_index << " target " << child_threadindex << endl;
    fflush(stdout);
#endif


    // When child is not finished, current thread should wait on cond var until child is exited.
    // It is possible that children has been exited, then it will make sure this.
    determ::getInstance().join(child_threadindex, _thread_index, wakeupChildren);
    
    determ_task_clock_add_ticks(fast_forward_clock());

    xmemory::wake();

    commitAndUpdateMemory();

#ifdef PRINT_SCHEDULE
    cout << "SCHED: FINISHED JOIN - tid: " << _thread_index << " target " << child_threadindex << endl;
    fflush(stdout);
#endif

    
    // Release the token.
    putToken();
    
    // Cleanup some status about the joinee.  
    xthread::join(v, result);

    startClock();
  }

  /// @brief Do a pthread_cancel
  static inline void cancel(void *v) {
    int threadindex;
    stopClockNoCoarsen();
    waitToken();
    commitAndUpdateMemory();
    threadindex = xthread::cancel(v);
    determ::getInstance().cancel(threadindex);
#ifdef PRINT_SCHEDULE
    cout << "SCHED: CANCEL THREAD - tid: " << _thread_index << " target " << threadindex << endl;
    fflush(stdout);
#endif
    putToken();
    startClock();
  }

    /* Heap-related functions. */
  static inline void * malloc(size_t sz) {
      return conseq_malloc::malloc(sz);
  }
  static inline void * calloc(size_t nmemb, size_t sz) {
      return conseq_malloc::calloc(nmemb, sz);
  }
  static inline void free(void * ptr) {
      return conseq_malloc::free(ptr);
  }
  static inline size_t getSize(void * ptr) {
      return conseq_malloc::getSize(ptr);
  }
  static inline void * realloc(void * ptr, size_t sz) {
      return conseq_malloc::realloc(ptr,sz);
  }


    ///// conditional variable functions.
  static void cond_init(void * cond) {
      stopClock();
      waitToken();
      determ::getInstance().cond_init(cond);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif
      commitAndUpdateMemory();
      putToken();
      startClock();
  }


  static void cond_destroy(void * cond) {
    determ::getInstance().cond_destroy(cond);
  }

  // Barrier support
  static int barrier_init(pthread_barrier_t *barrier, unsigned int count) {
      stopClock();
      waitToken();
      determ::getInstance().barrier_init(barrier, count);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: BARRIER INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(barrier) << endl;
      fflush(stdout);
#endif
      commitAndUpdateMemory();
      putToken();
      startClock();
      return 0;
  }

  static int barrier_destroy(pthread_barrier_t *barrier) {
    determ::getInstance().barrier_destroy(barrier);
    return 0;
  }

  ///// mutex functions
  /// FIXME: maybe it is better to save those actual mutex address in original mutex.
  static int mutex_init(pthread_mutex_t * mutex) {
      stopClock();
      waitToken();
      determ::getInstance().lock_init((void *)mutex);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: MUTEX INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << endl;
      fflush(stdout);
#endif
      commitAndUpdateMemory();
      putToken();
      startClock();
      return 0;
  }

    static u_int64_t fast_forward_clock(){
        int64_t clockDiff, clockDiffReturn;
        clockDiffReturn=0;
#ifdef FAST_FORWARD
        //if the token's clock is greater than ours
        u_int64_t lastClock = determ::getInstance().getLastTokenClock();
        //get the difference between the last token holder's clock and our clock
        clockDiff = lastClock - determ_task_clock_read();
        if (clockDiff > 0){
            clockDiffReturn=clockDiff;
        }
#endif
        return clockDiffReturn;
    }
    
    static int waitToken(void) {
      struct timespec t1,t2;
      int spin_counter=0;
      if (!_token_holding){
          spin_counter=determ::getInstance().getToken(_thread_index);
          //fast forward our clock
          determ_task_clock_add_ticks(fast_forward_clock());
          _token_holding=true;
          //we just got out of a coarsened tx...should we increase or decrease the granularity?
          if (tx_monitor_next){
              if(determ::getInstance().getLastTokenPutter()==_thread_index){
                  //did we have the token before? If we did, lets increase the coarsening level
                  tx_current_coarsening_level=xmin(tx_current_coarsening_level+tx_current_coarsening_level, LOGICAL_CLOCK_MAX_ALLOWABLE_TX_SIZE);
              }
              else{
                  //if we just got out of a coarsened tx and we didn't get the token next...cut it in half
                  tx_current_coarsening_level=LOGICAL_CLOCK_TX_SIZE_AFTER_TOKEN_TRANSFER;
              }
              tx_monitor_next=false;
          }
      }
      return spin_counter;
  }


    //stops the clock and ends any coarsening
    static void stopClockNoCoarsen(){
        stopClock();
        endTXCoarsening();
    }
    
    static void stopClock(size_t id){
        if (inCoarsenedTx()){
            determ_task_clock_stop_with_id_no_notify(id);
        }
        else{
            determ_task_clock_stop_with_id(id);
        }
    }

    static void stopClock(void){
        stopClock(0);
    }

    static void startClock(void){
        if (inCoarsenedTx()){
            determ_task_clock_start_no_notify();
        }
        else{
            determ_task_clock_start();       
        }
    }

    // If those threads sending out condsignal or condbroadcast,
    // we will use condvar here.
    static void putToken(void) {
        //reset the coarseing counter
#ifdef USE_TX_COARSENING
        if (tx_coarsening_counter > 0){
            endTXCoarsening();
        }
#endif

        if (_token_holding){
            // release the token and pass the token to next.
            //fprintf(stderr, "%d: putToken\n", _thread_index);
            determ::getInstance().putToken(_thread_index);
            _token_holding=false;
        }
        //  fprintf(stderr, "%d: putToken\n", getpid());
    }

    static void putTokenNoFastForward(){
        if (_token_holding){
            determ::getInstance().putTokenNoFastForward(_thread_index);
            _token_holding=false;
        }
    }

    static int __ticks_to_add(struct local_copy_stats * cs){
        int ticks=0;
        if (cs){
            ticks+=cs->partial_unique*LOGICAL_CLOCK_CONVERSION_UPDATE_PAGE + 
                cs->dirty_pages*LOGICAL_CLOCK_CONVERSION_COMMIT_PAGE + 
                cs->merged_pages*LOGICAL_CLOCK_CONVERSION_MERGE_PAGE;
        }

        ticks+=fast_forward_clock();

        return ticks;
    }

#ifdef USE_SIMPLE_LOCKS
    static void __mutex_lock_inner(pthread_mutex_t * mutex, bool allow_coarsening) {
        //we only need to update if we don't currently have the token. This is to optimize for nested locks
        bool need_to_update=false;
        if (!_token_holding){
            need_to_update=true;
        }
        waitToken();
        _lock_count++;
#ifdef TOKEN_ORDER_ROUND_ROBIN
        determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
        //lets actually get the "real" lock, which is really just setting a flag
        bool getLock=determ::getInstance().lock_acquire(mutex,_thread_index);
        if (!getLock){
            cout << "Something weird happened" << endl;
        }
        if (need_to_update){
            commitAndUpdateMemory();
        }
#ifdef PRINT_SCHEDULE
      cout << "SCHED: MUTEX LOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << endl;
      fflush(stdout);
#endif

        determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_LOCK, (void *)_token_holding);
    }


#else
    static void __mutex_lock_inner(pthread_mutex_t * mutex, bool allow_coarsening) {
        struct local_copy_stats cs;
        bool isSingleActiveThread=false;
        int failure_count=0;
        _lock_count++;
        //should we use the tx coarsening?
        bool isUsingTxCoarsening=(useTxCoarsening((size_t)mutex) && allow_coarsening);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "LOCK: starting lock " << determ_task_get_id() << " " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif
    retry:
        //if we are using kendo, we have to keep retrying and incrementing
        //if we aren't using kendo, this is just initialized to zero
        int ticks_to_add=KENDO_ACQ_INC;
        isSingleActiveThread=singleActiveThread();
        //get the token, assuming its not just us and we don't already own it
        if ((!isSingleActiveThread && !_token_holding) || failure_count>0) {
            waitToken();
        }
        //even if we are using coarsening, we may need to update before we hold on to the token and keep going
        //***this needs to happen AFTER we get the token
        bool shouldUpdate=(_thread_index!=determ::getInstance().getLastTokenPutter());
        //lets actually get the "real" lock, which is really just setting a flag
        bool getLock=determ::getInstance().lock_acquire(mutex,_thread_index);
        //the lock was taken, we need to keep trying
        if(getLock == false) {
            failure_count++;
            if (isSingleActiveThread){
                cout << "ERROR: Lock failed with single active thread..." << endl;
                exit(-1);
            }
            //some one else is going to get the token now. We need to commit our changes to memory now since we may be a coarse tx
            if (failure_count==1){
                commitAndUpdateMemory(&cs);
            }
            //reset the coarsening counter
            //tx_coarsening_counter=0;
            endTXCoarsening();
            isUsingTxCoarsening=false;
#ifdef USE_KENDO
            //need to add ticks for Kendo
            determ_task_clock_add_ticks(ticks_to_add);
            putToken();
#else
            determ::getInstance().wait_on_lock_and_release_token(mutex, _thread_index);
            _token_holding=false;
#endif
            goto retry;
        }
        else if ((!isSingleActiveThread && !isUsingTxCoarsening)||shouldUpdate){
            //determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_COMMIT, mutex);
            commitAndUpdateMemory(&cs);
            ticks_to_add+=__ticks_to_add(&cs) + LOGICAL_CLOCK_TIME_LOCK;
#ifdef DTHREADS_TASKCLOCK_DEBUG
            cout << "IN-LOCK for thread " << _thread_index << " pid " << getpid() << " partial " << cs.partial_unique << " dirty " << cs.dirty_pages << " merged " << 
                cs.merged_pages << " fast forward " << fast_forward_clock() << " total ticks " << ticks_to_add << endl;          
#endif
            //determ::getInstance().add_event_commit_stats(_thread_index, xmemory::get_updated_pages(), cs.merged_pages, cs.partial_unique, cs.dirty_pages);
            //determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_COMMIT);
            //important LoC, since if the lock is taken this will ensure progress
            determ_task_clock_add_ticks(ticks_to_add);
        }

        
#ifdef TOKEN_ORDER_ROUND_ROBIN
        determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif

#ifdef PRINT_SCHEDULE
        cout << "SCHED: MUTEX LOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << " " 
             << determ_task_clock_read() << endl;
        fflush(stdout);
#endif

        //release the token if need be
        if (!isSingleActiveThread && !isUsingTxCoarsening){
            putToken();
        }
    }

#endif

    static void mutex_lock(pthread_mutex_t * mutex) {

        uint64_t clock1,clock2;        
        timespec t1,t2;

        //**************DEBUG CODE**************
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
        //*************END DEBUG CODE*********************

        clock1=determ_task_clock_read();
        stopClock();
        //*****DEBUG CODE************************/
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
        //******************************************/

#ifdef USE_TAGGING
        xmemory::set_local_version_tag((unsigned int)mutex);
#endif
        __mutex_lock_inner(mutex, true /*allow coarseing?*/);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "mutex lock " << _thread_index << " " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif        
        //**************DEBUG CODE**************
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
        //*************END DEBUG CODE*********************
        startClock();

        //*****DEBUG CODE************************/
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
        //******************************************/

        //**************DEBUG CODE*********************
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, mutex);        
        //*************END DEBUG CODE*********************

    }


#ifdef USE_SIMPLE_LOCKS

    static void mutex_unlock(pthread_mutex_t * mutex) {
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
        stopClock((size_t)mutex);
        if (!_token_holding){
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_UNLOCK, mutex);
        }
        else{
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_UNLOCK+100, mutex);
        }
        _lock_count--;
        determ::getInstance().lock_release(mutex,_thread_index);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: MUTEX UNLOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << endl;
      fflush(stdout);
#endif

        if (_lock_count==0){
            commitAndUpdateMemory();
#ifdef TOKEN_ORDER_ROUND_ROBIN
            determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
            putToken();
        }
        startClock();
    }
    
#else
    
  static void mutex_unlock(pthread_mutex_t * mutex) {
      //**************DEBUG CODE**************
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      //**************************************
      //**************DEBUG CODE**************
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
      //*************END DEBUG CODE*********************
      stopClock((size_t)mutex);
      bool isSingleActiveThread=singleActiveThread();
      bool isUsingTxCoarsening=useTxCoarsening(0);
#ifdef DTHREADS_TASKCLOCK_DEBUG
      cout << "starting unlock " << _thread_index << " " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif
      _lock_count--;
      //*****DEBUG CODE************************/
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
      //******************************************/

      //if we're not the only one around, grab the token
      if (!isSingleActiveThread && !_token_holding){
          //get the token
          waitToken();
      }
      //even if we are using coarsening, we may need to update before we hold on to the token and keep going
      //***this needs to happen AFTER we get the token*******
      bool shouldUpdate=(_thread_index!=determ::getInstance().getLastTokenPutter());

      //if we're not using coarsening, and there's a waiting thread, then we have to commit. This covers the
      //case when we are the only thread alive, but once we release the token we will wake up someone else. If
      //we don't commit they will get an outdated version of memory.
      //******This needs to happen AFTER we get the token!!!*****
      bool commitForWaitingThread=(determ::getInstance().lock_waiters_count(mutex) > 0 && !isUsingTxCoarsening);

      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_UNLOCK, mutex);
      //*************END DEBUG CODE*********************
      if ((!singleActiveThread() && !isUsingTxCoarsening) || shouldUpdate || commitForWaitingThread){
          determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_COMMIT, mutex);
          commitAndUpdateMemory();
          determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_COMMIT);
      }
      //**************DEBUG CODE**************
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
      //*************END DEBUG CODE*********************
      // Unlock current lock.
      determ::getInstance().lock_release(mutex,_thread_index);
      //if we are using RR, add a large number to our clock
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
#ifdef PRINT_SCHEDULE
      cout << "SCHED: MUTEX UNLOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << endl;
      fflush(stdout);
#endif
      if (!isSingleActiveThread && !isUsingTxCoarsening){
          //release the token
          putToken();
      }

#ifdef USE_TAGGING
      xmemory::set_local_version_tag(0);
#endif
      //*****DEBUG CODE************************/
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
      //******************************************/
      //**************DEBUG CODE**************
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
      //*************END DEBUG CODE*********************
      startClock();
  }

#endif

    
  static int mutex_destroy(pthread_mutex_t * mutex) {
    determ::getInstance().lock_destroy(mutex);
    return 0;
  }

  // Add the barrier support.
  static int barrier_wait(pthread_barrier_t *barrier) {
      stopClock();
#ifdef USE_TAGGING
      xmemory::set_local_version_tag((unsigned int)barrier);
#endif
      //we acquire the token as a group...the only way this code will fire is if a tx is coarsened
      //and leads into a barrier
      putToken();
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      determ::getInstance().barrier_wait(barrier, _thread_index);
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
#ifdef USE_TAGGING
      xmemory::set_local_version_tag(0);
#endif
      startClock();
  }
  
  static void cond_wait(void * cond, void * lock) {
      stopClock();
      bool acquiringToken=(!_token_holding);
      //**************DEBUG CODE**************
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_WAIT, cond);
      //*************END DEBUG CODE*********************
#ifdef USE_TAGGING
        xmemory::set_local_version_tag((unsigned int)lock);
#endif
      _lock_count--;
      if (acquiringToken){
          waitToken();
      }
      commitAndUpdateMemory();
      
      //TODO: We need a better solution for this...this is embarassing :)
      if (sleep_count<MAX_SLEEP_COUNT){
          xmemory::sleep();
          is_sleeping=true;
          sleep_count++;
      }
      //we will release the token in here, make sure to end a coarsened transaction
      endTXCoarsening();
#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND WAIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif
      determ::getInstance().cond_wait(_thread_index, cond, lock);
      if (is_sleeping){
          xmemory::wake();
          is_sleeping=false;
      }

      //**************DEBUG CODE**************
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_WOKE_UP, cond);
      //*************END DEBUG CODE*********************
      //token gets released inside cond_wait()...lets make sure we make that here
      _token_holding=false;
      commitAndUpdateMemory();
      __mutex_lock_inner((pthread_mutex_t *)lock, false /*allow coarsening/*/);
#ifdef USE_TAGGING
        xmemory::set_local_version_tag(0);
#endif
      startClock();
  }
  

  static void cond_broadcast(void * cond) {
      stopClock();
      //if we are in a coarse tx, we're about to signal another thread...so reset it
      resetTXCoarsening();
      //if we don't already own the token, we need to commit. 
      bool acquiringToken=(!_token_holding);
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      waitToken();
      commitAndUpdateMemory();
      //**************DEBUG CODE**************
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_SIG, cond);
      //*************END DEBUG CODE*********************
      determ::getInstance().cond_broadcast(_thread_index, cond);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND BROADCAST - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif

      putToken();
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
      startClock();
  }

  static void cond_signal(void * cond) {
      stopClock();
      //if we are in a coarse tx, we're about to signal another thread...so reset it
      resetTXCoarsening();
      //if we don't already own the token, we need to commit. 
      bool acquiringToken=(!_token_holding);
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      if (acquiringToken){
          waitToken();
          commitAndUpdateMemory();
      }
      //**************DEBUG CODE**************
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_SIG, cond);
      //*************END DEBUG CODE*********************
      determ::getInstance().cond_signal(_thread_index, cond);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND SIGNAL - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif

      if (acquiringToken){
          putToken();
      }
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
      startClock();
  }

    static void commitAndUpdateMemory(){
        commitAndUpdateMemory(NULL);
    }

    static void commitAndUpdateMemory(struct local_copy_stats * stats){
        determ::getInstance().commitInSerial(_thread_index,stats);
    }

    static void commitAndUpdateMemoryParallelBegin(){
        commitAndUpdateMemoryParallelBegin(NULL);
    }
    
    static void commitAndUpdateMemoryParallelBegin(struct local_copy_stats * stats){
        determ::getInstance().commitAndUpdateMemoryParallelBegin(_thread_index, stats, &heapVersionToWaitFor, &globalsVersionToWaitFor);
    }

    static void commitAndUpdateMemoryParallelEnd(){
        commitAndUpdateMemoryParallelEnd(NULL);
    }
        
    static void commitAndUpdateMemoryParallelEnd(struct local_copy_stats * stats){
        determ::getInstance().commitAndUpdateMemoryParallelEnd(_thread_index, stats, heapVersionToWaitFor, globalsVersionToWaitFor);
    }
    
    static void sigstopHandle(int signum, siginfo_t * siginfo, void * context) {
        stopClock();
        waitToken();
        commitAndUpdateMemory();
        putToken();
        startClock();
    }


    static void installSignalHandler(void) {
        struct sigaction siga;
        sigemptyset(&siga.sa_mask);
        sigaddset(&siga.sa_mask, SIGUSR1);
        sigprocmask(SIG_BLOCK, &siga.sa_mask, NULL);
        siga.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
        siga.sa_sigaction = sigstopHandle;
        if (sigaction(SIGUSR1, &siga, NULL) == -1) {
            perror("sigaction(SIGUSR1)");
            exit(-1);
        }
        sigprocmask(SIG_UNBLOCK, &siga.sa_mask, NULL);

    }
};

#endif
