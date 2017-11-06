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
#include "speculation.h"
#include "debug_user_memory.h"
#include <sys/resource.h>
#include <determ_clock.h>
#include <signal.h>
#include <errno.h>

#define MAX_SLEEP_COUNT 10

#define DEBUG_STACK_ADDRESS 0xffffea20
#define DEBUG_ARRAY_ADDRESS 0x80a8090

#ifndef TOKEN_ACQ_ADD_CLOCK
#define TOKEN_ACQ_ADD_CLOCK 10000
#endif

class xrun {


private:
    static volatile bool _initialized;
    static size_t _master_thread_id;
    static size_t _thread_index;
    static size_t _lock_count;
    static bool _token_holding;
    //the number of dirty pages used to increase a threads logical clock during speculation
    static int spec_dirty_count;
    static unsigned long long last_cycle_read;
    static unsigned long long wait_cycles;
    
    static int reverts;
    static int locks_elided;
    static int token_acq;
    
    /******these variables keep track of some of the coarsening thread-local state*/
    static int tx_coarsening_counter;
    static int tx_current_coarsening_level;
    static int tx_consecutively_coarsened;
    static bool tx_monitor_next;
    /************************************************/

    //*************object for speculation*********************
    static speculation * _speculation;
    //********************************************

    /********Sleeping is done to make things easier on the Conversion garbage collector. 
    The GC is not concurrent, and does not collect versions that are newer than the oldest
    version currently held. In the event that the main thread creates some children, and then
    joins on them...and thus holding on to an early version...then no versions are ever 
    collected! Instead, we put the thread to "sleep" and that allows the GC to collect newer
    versions. The sleep_count is used to ensure we don't sleep/unsleep too much. We really don't
    want to do it too much as it adds some overhead.****/

    static int sleep_count;
    static bool is_sleeping;
    static bool alive;

    static uint64_t heapVersionToWaitFor;
    static uint64_t globalsVersionToWaitFor;

    static uint64_t _last_token_release_time;

    //statistics stuff
    static int characterize_lock_count, characterize_lock_count_spec, characterize_barrier_wait, characterize_lock_count_spec_fast_path;
    static int spec_signals_count, signals_count;
    
    static size_t monitor_address;

    static debug_user_memory * debug_mem;
    
public:

  /// @brief Initialize the system.
  static void initialize(void) {

    void* buf = WRAP(mmap)(NULL, sizeof(speculation), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    _speculation = new (buf) speculation(0);
    buf = WRAP(mmap)(NULL, sizeof(debug_user_memory), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    debug_mem = new (buf) debug_user_memory(0);
    reverts=0;
    spec_dirty_count=0;
    locks_elided=0;
    token_acq=0;
    _initialized = false;
    _lock_count = 0;
    _token_holding = false;
    sleep_count=0;
    is_sleeping=false;
    tx_coarsening_counter=0;
    tx_consecutively_coarsened=0;
    heapVersionToWaitFor=0;
    globalsVersionToWaitFor=0;
    characterize_lock_count=0;
    characterize_lock_count_spec=0;
    characterize_lock_count_spec_fast_path=0;
    characterize_barrier_wait=0;
    spec_signals_count=0;
    signals_count=0;
    tx_current_coarsening_level=LOGICAL_CLOCK_MIN_ALLOWABLE_TX_SIZE;
    tx_monitor_next=false;

    char * monitor_addr_tmp=getenv("CONSEQ_MONITOR");
    if (monitor_addr_tmp && strlen(monitor_addr_tmp)>0){
        monitor_address=(size_t)strtol(monitor_addr_tmp, NULL, 16);
    }
    else{
        monitor_address=0xDEAD;
    }
    
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
    alive=true;
  }

    static bool singleActiveThread(void){

        return determ::getInstance().singleActiveThread(_thread_index);
        
        //#ifdef SINGLE_THREAD_OPT        
        //        return (determ_task_clock_single_active_thread() && (_thread_index==determ::getInstance().getLastTokenHolder()));
        //#else
        //        return false;
        //#endif
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
      cout << "specstats: " << characterize_lock_count << "," << characterize_lock_count_spec << "," <<
          signals_count << "," << spec_signals_count << "," <<
          characterize_barrier_wait << "," <<
          token_acq << "," <<_speculation->getReverts() << "," << _speculation->getCommits() << "," <<
          _speculation->meanRevertCS() << "," << _speculation->meanSpecCS() << endl;
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


    static int endSpeculation(void){
#ifdef EVENT_VIEWER
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK, NULL);
        if (_speculation->validate()){
            determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK);
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_END_SPECULATION, (void *)_speculation->getTerminateReasonType());
            return 1;
        }
#else
        return _speculation->validate(false);
#endif
    }
    
  // New created thread should call this.
  // Now only the current thread is active.
    static inline int childRegister(int pid, int parentindex, int child_index, bool newThread) {
    int threads;
    struct timespec t1,t2;


    clock_gettime(CLOCK_MONOTONIC, &t1);
    // Get the global thread index for this thread, which will be used internally.
    //_thread_index = xatomic::increment_and_return(&global_data->thread_index);
    _thread_index = child_index;
    _lock_count = 0;
    _token_holding = false;
    alive=true;
    reverts=0;
    locks_elided=0;
    
    void* buf = WRAP(mmap)(NULL, sizeof(_speculation), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //only do this if we've forked a new thread. 
    if (newThread){
        _speculation = new (_speculation) speculation(_thread_index);
    }
    
    debug_mem = new (debug_mem) debug_user_memory(_thread_index);
    
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
    determ::getInstance().active_threads_inc();
    return (_thread_index);
  }

  static void threadDeregister(void) {
      stopClockForceEnd();

      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
#ifdef DTHREADS_TASKCLOCK_DEBUG
      cout << "thread deregister " << determ_task_get_id() << " count " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif
      waitToken();
      commitAndUpdateMemoryTerminateSpeculation();

      debug_mem->print();
      
      if (determ::getInstance().is_master_thread_finisehd()){
          ThreadPool::getInstance().set_exit_by_id(_thread_index);
      }
      else{
          ThreadPool::getInstance().add_thread_to_pool_by_id(_thread_index);
      }

      cout << "specstats: locks: " << characterize_lock_count << ", spec locks: " << 
	  characterize_lock_count_spec << ", spec locks fastpath: " << 
          characterize_lock_count_spec_fast_path << ", signals: " <<
          signals_count << ", spec signals: " << spec_signals_count << ", barriers: " <<
          characterize_barrier_wait << "," <<
          token_acq << ", reverts: " <<_speculation->getReverts() << ", commits: " << _speculation->getCommits() << ", revertCSLength: " <<
          _speculation->meanRevertCS() << ", specCSLength: " << _speculation->meanSpecCS() << endl;
      
      xmemory::sleep();
      alive=false;
      //the token is released in here....
      determ::getInstance().deregisterThread(_thread_index);
      determ::getInstance().active_threads_dec();
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

    static inline uint64_t get_ticks_for_speculation(){
#ifdef NO_DETERM_SYNC
        return determ::getInstance().getTokenCounter();
#else
        return determ_task_clock_read();
#endif // NO_DETERM_SYNC
    }
    
  /// @brief Spawn a thread.
    static void * spawn(threadFunction * fn, void * arg, pthread_t * tid) {
        //we use this to designate the "end" of the user's stack. This i
        uint8_t end_of_user_stack_marker;

        stopClockForceEndNoCoarsen();
        
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);

        //now, lets initialize the thread so that there is not a race with it to get the token
        waitToken();
        //cout << "SPAWN: ending speculation " << getpid() << endl;
//      if (_lock_count>0){
//          cout << "FORKING WHILE HOLDING A LOCK...not currently supported" << endl;
//          exit(-1);
//      }

#ifdef PRINT_SCHEDULE
        cout << "SCHED: CREATING THREAD - tid: " << _thread_index << endl;
        fflush(stdout);
#endif
        //commit our memory so the spawned thread sees it
        commitAndUpdateMemoryTerminateSpeculation();
        //add the estimated time its going to take to fork a new process. 
        determ_task_clock_add_ticks(LOGICAL_CLOCK_TIME_FORK_PER_PAGE * xmemory::get_logical_pages());
        //get the thread pool entry. Need to do this while holding the token since we will wake up another thread
        ThreadPoolEntry * tpe = ThreadPool::getInstance().get_thread(fn, arg, (is_stack_addr(arg, &end_of_user_stack_marker) ? STACK_ALLOCATED_ARG : HEAP_OR_GLOBAL_ARG), 
                                                                               (void *)&end_of_user_stack_marker);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_FORKED_THREAD);
#endif
        //now release the token so other threads can keep on chuggin'
        //putTokenNoFastForward();
        putToken();
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
  static void join(void * v, void ** result) {
    int  child_threadindex = 0;
    bool wakeupChildren = false;

    stopClockForceEndNoCoarsen();

    // Return immediately if the thread argument is NULL.
    if (v == NULL) {
        cout << "join with invalid parameter" << endl;
        return;
    }

    // Wait on token if the fence is already started.
    // It is important to maitain the determinism by waiting. 
    // No need to wait when fence is not started since join is the first
    // synchronization after spawning, other thread should wait for 
    // the notification from me.
    waitToken();
    commitAndUpdateMemoryTerminateSpeculation();
    
    // Get the joinee's thread index.
    child_threadindex = xthread::getThreadIndex(v);

#ifdef PRINT_SCHEDULE
    cout << "SCHED: BEGIN JOIN - tid: " << _thread_index << " target " << child_threadindex << endl;
    fflush(stdout);
#endif

    // When child is not finished, current thread should wait on cond var until child is exited.
    // It is possible that children has been exited, then it will make sure this.
    determ::getInstance().join(child_threadindex, _thread_index, wakeupChildren);
    determ_task_clock_add_ticks(fast_forward_clock());
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
  static void cancel(void *v) {
    int threadindex;
    stopClockForceEndNoCoarsen();
    waitToken();
    commitAndUpdateMemoryTerminateSpeculation();
    determ::getInstance().cancel(threadindex);
    threadindex = xthread::cancel(v);
#ifdef PRINT_SCHEDULE
    cout << "SCHED: CANCEL THREAD - tid: " << _thread_index << " target " << threadindex << endl;
    fflush(stdout);
#endif
    putToken();
    startClock();
  }
    
    /* Heap-related functions. */
  static inline void * malloc(size_t sz) {
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MALLOC, (void *)sz);
      void * ptr = conseq_malloc::malloc(sz);
      if (ptr==NULL && _speculation->isSpeculating()){
          //we would get here if the thread-local heap tried to allocate from the shared heap
          stopClockForceEnd();
          waitToken();
          commitAndUpdateMemoryTerminateSpeculation();
          ptr = conseq_malloc::malloc(sz);
          if (ptr==NULL){
              cout << "whoops we are having an issue with malloc on speculation!!!!! " << endl;
              exit(-1);
          }
          startClock();
      }
      return ptr;
  }
    
  static inline void * calloc(size_t nmemb, size_t sz) {
      void * result=NULL;
      if (nmemb!=0 && sz!=0){
          result=malloc(nmemb * sz);
          memset(result, 0, nmemb*sz);
      }
      return result;
  }
    
  static inline void free(void * ptr) {
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_FREE, NULL);
      conseq_malloc::free(ptr);
  }

    static inline size_t getSize(void * ptr) {
        return conseq_malloc::getSize(ptr);
    }
    
  static inline void * realloc(void * ptr, size_t sz) {

      void * newptr = conseq_malloc::realloc(ptr, sz);
      if (newptr==NULL && _speculation->isSpeculating()){
          //we would get here if the thread-local heap tried to allocate from the shared heap
          stopClockForceEnd();
          waitToken();
          commitAndUpdateMemoryTerminateSpeculation();
          newptr = conseq_malloc::realloc(ptr, sz);
          if (newptr==NULL){
              cout << "whoops we are having an issue with realloc on speculation!!!!! " << endl;
              exit(-1);
          }
          startClock();
      }

      return newptr;
  }


    ///// conditional variable functions.
  static void cond_init(void * cond) {
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_INIT, (void *)cond);
      stopClock();
      bool skip = (singleActiveThread() || _speculation->isSpeculating()) ;
      if (!skip){
          waitToken();
      }
      determ::getInstance().cond_init(cond);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif
      if (!skip){
          commitAndUpdateMemory();
          putToken();
      }
      startClock();
  }


  static void cond_destroy(void * cond) {
    determ::getInstance().cond_destroy(cond);
  }

  // Barrier support
  static int barrier_init(pthread_barrier_t *barrier, unsigned int count) {
      stopClock();
      bool isSingleActiveThread = singleActiveThread();
      if (!isSingleActiveThread){
          waitToken();
      }
      determ::getInstance().barrier_init(barrier, count);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: BARRIER INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(barrier) << endl;
      fflush(stdout);
#endif
      if (!isSingleActiveThread){
          commitAndUpdateMemory();
          putToken();
      }
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
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_INIT, (void *)mutex);
      bool skip = (singleActiveThread() || _speculation->isSpeculating()) ;
      if (!skip){
          waitToken();
      }
      determ::getInstance().lock_init((void *)mutex);
#ifdef PRINT_SCHEDULE
      cout << "SCHED: MUTEX INIT - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << " " << determ_task_clock_read() << " " << determ_debug_notifying_clock_read() << endl;
      fflush(stdout);
#endif
      if (!skip){
          commitAndUpdateMemory();
          putToken();
      }
      startClock();
      return 0;
  }

    static u_int64_t fast_forward_clock(){
        int64_t clockDiff, clockDiffReturn;
        clockDiffReturn=0;
#ifdef FAST_FORWARD
        //if the token's clock is greater than ours.
        //we add one to ensure we avoid the situation where we wakeup a thread with a lower id
        //and it gets the same logical clock as the previous thread. So now it has the same clock
        //and a lower id. This may mess up our speculation which assumes a monotonically increasing
        //clock w.r.t token acquisition
        u_int64_t lastClock = determ::getInstance().getLastTokenClock() + 1;
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
          token_acq++;
#ifdef TRACK_LIBRARY_CYCLES
          unsigned long long start_cycles = determ_task_clock_read_cycle_counter();
#endif
          spin_counter=determ::getInstance().getToken(_thread_index);
#ifdef TRACK_LIBRARY_CYCLES
          wait_cycles = determ_task_clock_read_cycle_counter() - start_cycles;
#endif
          //cout << "ff tid: " << _thread_index << " " << fast_forward_clock() << endl;
          //fast forward our clock
          determ_task_clock_add_ticks(fast_forward_clock() + (TOKEN_ACQ_ADD_CLOCK * determ::getInstance().active_threads_get()));
          _token_holding=true;
          //cout << "got token tid: " << _thread_index << " " << determ_task_clock_read() << endl;
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

      int lastToken=determ::getInstance().getLastTokenPutter();
      return spin_counter;
  }


    static void stopClock(size_t id, bool forceOnSpec){
        if (inCoarsenedTx()){
            determ_task_clock_stop_with_id_no_notify(id);
        }
        else if (!_speculation->isSpeculating() || forceOnSpec){
            determ_task_clock_stop_with_id(id);
        }
#ifdef TRACK_LIBRARY_CYCLES
        last_cycle_read=determ_task_clock_read_cycle_counter();
#endif
    }

    static void stopClockForceEnd(){
        stopClock(0, true);
    }

    static void stopClockForceEndNoCoarsen(){
        stopClock(0, true);
        endTXCoarsening();
    }
    
    static void stopClock(size_t id){
        stopClock(id, false);
    }
    
    static void stopClock(void){
        stopClock(0);
    }

    static void startClock(void){
        startClock(false);
    }
    
    static void startClock(bool forceStartOnSpec){
#ifdef TRACK_LIBRARY_CYCLES
        unsigned long long lib_cycles=determ_task_clock_read_cycle_counter() - last_cycle_read;
        //sanity check
        if (lib_cycles < 10000000ULL && wait_cycles < 10000000ULL ){
            determ_task_clock_add_ticks_lazy(lib_cycles - wait_cycles);
        }
        lib_cycles = wait_cycles = 0;
#endif
        if (inCoarsenedTx()){
            determ_task_clock_start_no_notify();
        }
        else if (!_speculation->isSpeculating() || forceStartOnSpec){
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
            _last_token_release_time=determ_task_clock_read();
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

    static unsigned long long __rdtsc(void)
    {
        unsigned long low, high;
        asm volatile("rdtsc" : "=a" (low), "=d" (high));
        return ((low) | (high) << 32);
    }

    static void __mutex_lock_inner(pthread_mutex_t * mutex, bool allow_coarsening) {
        struct local_copy_stats cs;
        int wasSpeculating=0;
        bool finishCommit=false;
        bool isSingleActiveThread=false;
        int failure_count=0;
        int stack_lock_count=++_lock_count;
	unsigned long long begin, end;
	begin = __rdtsc();
retry:
        bool isSpeculating=_speculation->isSpeculating();
        //should we use the tx coarsening?
        bool isUsingTxCoarsening= !isSpeculating && useTxCoarsening((size_t)mutex) && allow_coarsening;
        //if we are using kendo, we have to keep retrying and incrementing
        //if we aren't using kendo, this is just initialized to zero
        int ticks_to_add=0;
        int shouldSpecResult=0;
        isSingleActiveThread= !isSpeculating && singleActiveThread();
        //We can't speculate when we are using coarsening, because we are already holding the lock and that
        //doesn't make much sense.

        //cout << "use coarsening??? " << isUsingTxCoarsening << " single active " << 
          //  isSingleActiveThread << " " << failure_count << " " << _lock_count << " pid " << getpid() << endl;

        if (!isSingleActiveThread && !isUsingTxCoarsening && (failure_count==0) &&
            !(isSpeculating==false && _lock_count>1) &&
            _speculation->shouldSpeculate(mutex, get_ticks_for_speculation(), &shouldSpecResult)){
            //Here we begin or continue speculation...in the event that a speculation is reverted we will
            //return false and continue on
#ifdef NO_DETERM_SYNC
            if (_speculation->speculate(mutex,get_ticks_for_speculation(), speculation::SPEC_ENTRY_LOCK)==true){
#else
                if (_speculation->speculate(mutex,_last_token_release_time, speculation::SPEC_ENTRY_LOCK)==true){
#endif
                if (!isSpeculating){
                    //HERE we know that we are beginning a speculation
                    spec_dirty_count=0;
                    //determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_TX_START, NULL);  
                    xmemory::begin_speculation();
                    //determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_BEGIN_SPECULATION, (void *)id);
                    //cout << "beginning spec lock count " << getpid() << endl;
                }
                characterize_lock_count_spec++;
                return;
            }
            else{
                _lock_count=stack_lock_count;
                //we just got back from a rolled back speculation
                reverts++;
            }
        }
        //the clock has been running this whole time...lets stop it before we try to grab the token (nondeterministic)
        if (isSpeculating){
            stopClock(0,true);
        }
        //get the token, assuming its not just us and we don't already own it
        if ((!isSingleActiveThread && !_token_holding) || failure_count>0) {
            waitToken();
        }
        else{
            //we need to update the last token acquisition time to our current logical clock, even though we don't actually
            //grab the token
            _last_token_release_time=determ_task_clock_read();
        }
        //even if we are using coarsening, we may need to update before we hold on to the token and keep going
        //***this needs to happen AFTER we get the token
        //Keep in mind, we do this after we acquire the lock, because we don't want to perform multiple updates
        bool shouldUpdate=(_thread_index!=determ::getInstance().getLastTokenPutter());
        //in the event that we were speculating lets do an update, release the token and try to speculate again
        if (wasSpeculating=endSpeculation()){
            locks_elided+=_speculation->getLockCount();
            //DEBUG_TYPE_SPECULATIVE_COMMIT
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_SPECULATIVE_COMMIT, NULL);
            determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_TX_ENDING, NULL);  
            _speculation->commitSpeculation(get_ticks_for_speculation());
            xmemory::end_speculation();
            commitAndUpdateMemoryParallelBegin();
            putToken();
            commitAndUpdateMemoryParallelEnd();
            isSpeculating=false;
            goto retry;
        }
        characterize_lock_count++;
        //lets actually get the "real" lock, which is really just setting a flag
        bool getLock=determ::getInstance().lock_acquire(mutex,_thread_index);
        //the lock was taken, we need to keep trying
        if(getLock == false) {
            failure_count++;
            if (isSingleActiveThread){
                cout << "ERROR: Lock failed with single active thread..." << endl;
                exit(-1);
            }
            //some one else is going to get the token now. We need to commit our changes to memory now since
            //we may be a coarse tx
            if (failure_count==1){
                commitAndUpdateMemory(&cs);
            }
            //reset the coarsening counter
            endTXCoarsening();
            isUsingTxCoarsening=false;
            determ::getInstance().wait_on_lock_and_release_token(mutex, _thread_index);
            _token_holding=false;
            goto retry;
        }
        else if ((!isSingleActiveThread && !isUsingTxCoarsening)||shouldUpdate){
            commitAndUpdateMemoryParallelBegin();
            finishCommit = true;
        }
#ifdef TOKEN_ORDER_ROUND_ROBIN
        determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
#ifdef PRINT_SCHEDULE
        cout << "SCHED: MUTEX LOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << " " 
             << determ_task_clock_read() << " " << determ_debug_notifying_clock_read() << endl;
        fflush(stdout);
#endif
        _speculation->updateLastCommittedTime(mutex,get_ticks_for_speculation());
        //release the token if need be
        if (!isSingleActiveThread && !isUsingTxCoarsening){
            putToken();
        }
	if (finishCommit){
	    commitAndUpdateMemoryParallelEnd();
        }
    }

    static inline void mutex_lock(pthread_mutex_t * mutex) {
//	unsigned long long start;
//	bool trace=false;
//	if (characterize_lock_count_spec_fast_path % 200 == 0) {
//	    start=__rdtsc();
//	    trace=true;
//	}
        bool isSpeculating = _speculation->isSpeculating();
        #ifdef NO_DETERM_SYNC
            if (false && isSpeculating && _speculation->shouldSpeculateFastPathLock(mutex, get_ticks_for_speculation())) {
        #else
            if (false && isSpeculating && _speculation->shouldSpeculateFastPathLock(mutex, _last_token_release_time)) {
        #endif
                characterize_lock_count_spec++;
		characterize_lock_count_spec_fast_path++;
                _lock_count++;
		//cout << "lock count fast path...." << _lock_count << " " << getpid() << endl;
//		if (trace) {
//		    cout << "fast " << __rdtsc() - start << endl;
//		}
		return;
        }
	stopClock();        
        //**************DEBUG CODE**************
#ifdef EVENT_VIEWER
        if (isSpeculating){
            determ::getInstance().add_event_commit_stats(_thread_index, 0, 0, 0, (xmemory::get_dirty_pages() - spec_dirty_count) );
        }
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
        determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_LOCK, mutex);
#endif
        //*************END DEBUG CODE*********************
#ifdef USE_TAGGING
        xmemory::set_local_version_tag((unsigned int)mutex);
#endif
        __mutex_lock_inner(mutex, true /*allow coarseing?*/);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "mutex lock " << _thread_index << " " << determ_task_clock_read() << " pid " << getpid() << " " << mutex << endl;
#endif
        //**************DEBUG CODE**************
#ifdef EVENT_VIEWER
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
#endif
        //*************END DEBUG CODE*********************        
        if (_speculation->isSpeculating() && _speculation->getEntriesCount()==1){
            //we just started a speculation and the clock is stopped. We need to make sure we start it but the
            //regular startClock(void) function does nothing if we are speculating.
            startClock(true);
        }
        else{
            startClock();
        }
        //*****DEBUG CODE************************/
#ifdef EVENT_VIEWER
        determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
        determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, mutex);
#endif
    }


  static void mutex_unlock(pthread_mutex_t * mutex) {
      //unsigned long long start = __rdtsc();
      stopClock((size_t)mutex);
      //**************DEBUG CODE**************
#ifdef EVENT_VIEWER
      if (_speculation->isSpeculating()){
          determ::getInstance().add_event_commit_stats(_thread_index, 0, 0, 0, (xmemory::get_dirty_pages() - spec_dirty_count) );
      }
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_UNLOCK, mutex);
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_LIB, mutex);
#endif
      //*************END DEBUG CODE*********************
      bool isSpeculating=_speculation->isSpeculating();            
      assert(_lock_count>0);
      _lock_count--;
#ifdef DTHREADS_TASKCLOCK_DEBUG
      cout << "UNLOCK: starting lock " << determ_task_get_id() << " " << determ_task_clock_read()
             << " tid " << _thread_index << " lock count " << _lock_count << " m: " << mutex << " " << getpid() << endl;
#endif

      if (isSpeculating){
          //we want to notify the speculation engine that we have released this lock
          _speculation->endSpeculativeEntry(mutex);
          //*****DEBUG CODE************************/
#ifdef EVENT_VIEWER
          determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
          determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_SPECULATIVE_UNLOCK, mutex);
          determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
#endif
          //*************END DEBUG CODE*********************
	  //unsigned long long end = __rdtsc();
	  //cout << "unlockSpec: " << end-start << " " << mutex << " " << getpid() << endl ;  
          return;
      }

      bool isSingleActiveThread=!isSpeculating && singleActiveThread();
      bool isUsingTxCoarsening=!isSpeculating && useTxCoarsening(0);
      bool finishCommit=false;
      
      //*****DEBUG CODE************************/
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_LIB);
      //******************************************/
      
      //if we're not the only one around, grab the token
      if (!isSingleActiveThread && !_token_holding){
          //get the token
          waitToken();
      }
      else{
          _last_token_release_time=determ_task_clock_read();
      }

      //even if we are using coarsening, we may need to update before we hold on to the token and keep going
      //***this needs to happen AFTER we get the token*******
      bool shouldUpdate=(_thread_index!=determ::getInstance().getLastTokenPutter());

      //if we're not using coarsening, and there's a waiting thread, then we have to commit. This covers the
      //case when we are the only thread alive, but once we release the token we will wake up someone else. If
      //we don't commit they will get an outdated version of memory.
      //******This needs to happen AFTER we get the token!!!*****
      bool commitForWaitingThread=(determ::getInstance().lock_waiters_count(mutex) > 0 && !isUsingTxCoarsening);

      //*****DEBUG CODE************************/
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_MUTEX_UNLOCK, mutex);
      //*************END DEBUG CODE*********************
      if ((!singleActiveThread() && !isUsingTxCoarsening) || shouldUpdate || commitForWaitingThread){
          determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_COMMIT, mutex);
          commitAndUpdateMemoryParallelBegin();
          finishCommit=true;
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
      cout << "SCHED: MUTEX UNLOCK - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(mutex) << " " << determ_task_clock_read() << " " << determ_debug_notifying_clock_read() << endl;
      fflush(stdout);
#endif


      _speculation->updateLastCommittedTime(mutex,get_ticks_for_speculation());


      if (!isSingleActiveThread && !isUsingTxCoarsening){
          //release the token
          putToken();
      }

      if (finishCommit){
          commitAndUpdateMemoryParallelEnd();
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
      //unsigned long long end = __rdtsc();
      //cout << "unlockNonSpec: " << end-start << " " << mutex << " " << getpid() << endl;
  }

    
  static int mutex_destroy(pthread_mutex_t * mutex) {
      cout << "NOT CURRENTLY SUPPORTED" << endl;
      exit(-1);
      //determ::getInstance().lock_destroy(mutex);
      return 0;
  }

  // Add the barrier support.
  static int barrier_wait(pthread_barrier_t *barrier) {
      stopClockForceEnd();
#ifdef USE_TAGGING
      xmemory::set_local_version_tag((unsigned int)barrier);
#endif

      characterize_barrier_wait++;
      /*//we acquire the token as a group...the only way this code will fire is if a tx is coarsened
      //and leads into a barrier
      putToken();*/
      //lets get the token...
      waitToken();
      if (_speculation->isSpeculating()){
          terminateSpeculation(true);
      }
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      determ::getInstance().barrier_wait(barrier, _thread_index);
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
#ifdef USE_TAGGING
      xmemory::set_local_version_tag(0);
#endif
      
      _token_holding=false;
      startClock();
      return 0;
  }
  
  static int cond_wait(void * cond, void * lock) {
      stopClockForceEnd();
      bool acquiringToken=(!_token_holding);
      bool wasSpeculating;
      
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
      //from the perspective of the speculation engine, we need to remove this lock from the
      //set of active locks. If we don't, it will prevent us from successfully committing our tx
      wasSpeculating=_speculation->isSpeculating();
      if (wasSpeculating){
          _speculation->endSpeculativeEntry(lock);
      }
      commitAndUpdateMemoryTerminateSpeculation();
      //in the event that we were speculating, the update to the condition guarding the cond_wait may have been
      //delayed. Its possible that the signaling thread has done the signal and is currently blocked, leading
      //to deadlock. So now that we've updated our view of memory, we need to create a "spurious" wakeup to make the
      //user code check again.
      //if (wasSpeculating){
      //  goto wokeup;
      //}
      
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

  wokeup:
      
      //**************DEBUG CODE**************
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_WOKE_UP, cond);
      //*************END DEBUG CODE*********************
      //token gets released inside cond_wait()...lets make sure we make that here
      _token_holding=false;
      __mutex_lock_inner((pthread_mutex_t *)lock, false /*allow coarsening/*/);
#ifdef USE_TAGGING
        xmemory::set_local_version_tag(0);
#endif
      startClock();
      return 0;
  }
  

  static void cond_broadcast(void * cond) {
      int shouldSpecResult;
      bool wasSpeculating;
      
      stopClock();

#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND BROADCAST - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif
      wasSpeculating=_speculation->isSpeculating();

      /*speculative path*/
      if(!(wasSpeculating==false && _lock_count>0) &&
         _speculation->shouldSpeculate(cond, get_ticks_for_speculation(),  &shouldSpecResult)){
#ifdef NO_DETERM_SYNC
          if (_speculation->speculate(cond,get_ticks_for_speculation(), speculation::SPEC_ENTRY_BROADCAST)==true){
#else
              if (_speculation->speculate(cond,_last_token_release_time, speculation::SPEC_ENTRY_BROADCAST)==true){
#endif
                  if (!wasSpeculating){
                      //HERE we know that we are beginning a speculation
                      spec_dirty_count=0;
                      xmemory::begin_speculation();
                      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_BEGIN_SPECULATION, (void *)id);
                      startClock(true);
                  }
                  else{
                      startClock();
                  }
                  spec_signals_count++;
                  return;
              }
              else{
                  determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK);
                  //we just got back from a rolled back speculation
                  determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_FAILED_SPECULATION, (void *)id);
                  reverts++;
              }
      }
      /*endspeculative path*/
      signals_count++;
      stopClockForceEnd();
      //if we are in a coarse tx, we're about to signal another thread...so reset it
      resetTXCoarsening();
      //if we don't already own the token, we need to commit. 
      bool acquiringToken=(!_token_holding);
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      waitToken();
      commitAndUpdateMemoryTerminateSpeculation();
      //**************DEBUG CODE**************
      determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_COND_SIG, cond);
      //*************END DEBUG CODE*********************
      determ::getInstance().cond_broadcast(_thread_index, cond);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
      putToken();
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION, NULL);
      startClock();
  }

  static void cond_signal(void * cond) {
      int shouldSpecResult;

#ifdef PRINT_SCHEDULE
      cout << "SCHED: COND SIGNAL - tid: " << _thread_index << " var: " << determ::getInstance().get_syncvar_id(cond) << endl;
      fflush(stdout);
#endif

      
      stopClock();
      if (_speculation->isSpeculating()){
          //should we continue speculating??
          if (_speculation->shouldSpeculate(cond, get_ticks_for_speculation(),  &shouldSpecResult)){
              //ready to add our entry
              _speculation->speculate(cond, _last_token_release_time, speculation::SPEC_ENTRY_SIGNAL);
              spec_signals_count++;
              return;
          }
      }

      signals_count++;
      stopClockForceEnd();
      //if we are in a coarse tx, we're about to signal another thread...so reset it
      resetTXCoarsening();
      //if we don't already own the token, we need to commit. 
      bool acquiringToken=(!_token_holding);
      determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_TRANSACTION);
      if (acquiringToken){
          waitToken();
          commitAndUpdateMemoryTerminateSpeculation();
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

  static void beginSysCall(){
      if (_initialized && alive){
          stopClockForceEnd();
          waitToken();
          if (_speculation && _speculation->isSpeculating()){
              cout << "beginSysSpec " << _speculation->getLogicalClockStart() << " " << getpid() << endl;
              commitAndUpdateMemoryTerminateSpeculationForced();
              endTXCoarsening();
          }
      }
  }


  static void endSysCall(){
      if (_initialized && alive){
          putToken();
          startClock();
      }
  }


    static void beginSysCallDeactivate(){
      if (_initialized && alive){
          stopClockForceEnd();
          bool acquiringToken=(!_token_holding);
          if (acquiringToken){
              waitToken();
          }          
          commitAndUpdateMemoryTerminateSpeculation();
          endTXCoarsening();
          determ_task_clock_halt();
          putToken();
      }
  }


  static void endSysCallActivate(){
      if (_initialized && alive){
          determ_task_clock_activate();
          determ_task_clock_on_wakeup();
          startClock();
      }
  }


  
  static void commitAndUpdateMemory(){
      assert(!_speculation->isSpeculating());
      commitAndUpdateMemory(NULL);
  }


  static void terminateSpeculation(bool forcedTerminate){
      determ::getInstance().start_thread_event(_thread_index, DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK, NULL);
      if (_speculation->validate(forcedTerminate)){
          determ::getInstance().end_thread_event(_thread_index, DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK);
          determ::getInstance().add_atomic_event(_thread_index, DEBUG_TYPE_END_SPECULATION, (void *)_speculation->getTerminateReasonType());
          locks_elided+=_speculation->getEntriesCount();
          _speculation->commitSpeculation(get_ticks_for_speculation());
          xmemory::end_speculation();
      }
  }
  
    static void commitAndUpdateMemoryTerminateSpeculation(){
        if (_speculation->isSpeculating()){
            terminateSpeculation(false);
        }
        commitAndUpdateMemory();
    }

    static void commitAndUpdateMemoryTerminateSpeculationParallel(){
        if (_speculation->isSpeculating()){
            terminateSpeculation(false);
        }
        commitAndUpdateMemoryParallelBegin();
    }

    static void commitAndUpdateMemoryTerminateSpeculationForced(){
        if (_speculation->isSpeculating()){
            terminateSpeculation(true);
        }
        commitAndUpdateMemory();
    }

    
    static void commitAndUpdateMemoryTerminateSpeculationParallelForced(){
        if (_speculation->isSpeculating()){
            terminateSpeculation(true);
        }
        commitAndUpdateMemoryParallelBegin();
    }

    
    
    static void commitAndUpdateMemory(struct local_copy_stats * stats){
        assert(!_speculation->isSpeculating());
        determ::getInstance().commitInSerial(_thread_index,stats);
    }

    /***PARALLEL COMMIT FUNCTIONS***/
    
    static void commitAndUpdateMemoryParallelBegin(){
        assert(!_speculation->isSpeculating());
#ifdef DISABLE_PARALLEL_COMMITS
        commitAndUpdateMemory();
#else
        commitAndUpdateMemoryParallelBegin(NULL);
#endif
    }
    static void commitAndUpdateMemoryParallelBegin(struct local_copy_stats * stats){
        assert(!_speculation->isSpeculating());
#ifdef DISABLE_PARALLEL_COMMITS
        commitAndUpdateMemory(stats);
#else
        determ::getInstance().commitAndUpdateMemoryParallelBegin(_thread_index, stats, &heapVersionToWaitFor, &globalsVersionToWaitFor);
#endif
    }
    static void commitAndUpdateMemoryParallelEnd(){
        assert(!_speculation->isSpeculating());
#ifdef DISABLE_PARALLEL_COMMITS
        
#else
        commitAndUpdateMemoryParallelEnd(NULL);
#endif
    }
    static void commitAndUpdateMemoryParallelEnd(struct local_copy_stats * stats){
        assert(!_speculation->isSpeculating());
#ifdef DISABLE_PARALLEL_COMMITS

#else
        determ::getInstance().commitAndUpdateMemoryParallelEnd(_thread_index, stats, heapVersionToWaitFor, globalsVersionToWaitFor);
#endif
    }

    /***END PARALLEL COMMIT***/
    
    static void sigstopHandle(int signum, siginfo_t * siginfo, void * context) {
        if (_speculation){
            cout << "specstats: locks: " << characterize_lock_count << ", spec locks: " << 
                characterize_lock_count_spec << ", spec locks fastpath: " << 
                characterize_lock_count_spec_fast_path << ", signals: " <<
                signals_count << ", spec signals: " << spec_signals_count << ", barriers: " <<
                characterize_barrier_wait << "," <<
                token_acq << ", reverts: " <<_speculation->getReverts() << ", commits: " << _speculation->getCommits() << ", revertCSLength: " <<
                _speculation->meanRevertCS() << ", specCSLength: " << _speculation->meanSpecCS() << endl;
        }
    }


    static void installSignalHandler(void) {
        struct sigaction siga;
        sigemptyset(&siga.sa_mask);
        sigaddset(&siga.sa_mask, SIGUSR1);
        sigprocmask(SIG_BLOCK, &siga.sa_mask, NULL);
        siga.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
        siga.sa_sigaction = sigstopHandle;
        if (sigaction(SIGUSR1, &siga, NULL) == -1) {
            exit(-1);
        }
        sigprocmask(SIG_UNBLOCK, &siga.sa_mask, NULL);

    }

#define SCHED_YIELD_WAIT 100000
    
    static void schedYield(void){
        //wait for a bit to grab the token
        for (int i=0;i<SCHED_YIELD_WAIT;i++){
            Pause();
        }
        stopClockForceEnd();
        waitToken();
        commitAndUpdateMemoryTerminateSpeculationParallel();
        putToken();
        commitAndUpdateMemoryParallelEnd();
        startClock();
    }
};

#endif
