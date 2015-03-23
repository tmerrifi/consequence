#ifndef __DETERM_H__
#define __DETERM_H__

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
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * @file   determ.h
 * @brief  Main file for determinism management.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 * @author Tim Merrifield <http://www.cs.uic.edu/Bits/TimothyMerrifield>
 */

#include <map>

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#endif

#include "sync.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "xdefines.h"
#include "list.h"
#include "xbitmap.h"
#include "xdefines.h"
#include "internalheap.h"
#include "real.h"
#include "prof.h"
#include "stats.h"
#include "logical_clock.h"
#include <determ_clock.h>
#include <sched.h>

#define MAX_THREADS 2048
#ifdef EVENT_VIEWER
#define MAX_EVENTS 12000
#else
#define MAX_EVENTS 0
#endif

//#define fprintf(...) 

#define BARRIER_TOKEN_HELD_FLAG (1<<20)

enum debug_event_type{
    DEBUG_TYPE_TRANSACTION = 0, DEBUG_TYPE_TOKEN_WAIT=1, DEBUG_TYPE_COMMIT=2, 
    DEBUG_TYPE_WAIT_LOWEST=3, DEBUG_TYPE_BARRIER_WAIT=4, DEBUG_TYPE_LOCK_FAILED=5, DEBUG_TYPE_WAIT_ON_COND=6, DEBUG_TYPE_LIB=7, DEBUG_TYPE_FORK=8,
    DEBUG_TYPE_COND_SIG=20, DEBUG_TYPE_COND_WAIT=21, DEBUG_TYPE_COND_WOKE_UP=22,
    DEBUG_TYPE_MUTEX_LOCK=23, DEBUG_TYPE_MUTEX_UNLOCK=24, DEBUG_TYPE_TOKEN_FAILED=25, DEBUG_TYPE_LOCK_SPIN_WAKE=26, DEBUG_TYPE_LOCK_CONDVAR_WAKE=27,
    DEBUG_TYPE_TX_COARSE_SUCCESS=28, DEBUG_TYPE_TX_COARSE_FAILED=29, DEBUG_TYPE_TX_ENDING=30, DEBUG_TYPE_TX_START=31, DEBUG_TYPE_MALLOC=32,
    DEBUG_TYPE_STOP_CLOCK_NOC=33, DEBUG_TYPE_STOP_CLOCK=34, DEBUG_TYPE_START_CLOCK_NOC=35, DEBUG_TYPE_START_CLOCK=36, DEBUG_TYPE_START_COARSE=37, DEBUG_TYPE_END_COARSE=38
};





//const char * debug_event_type_names[] = { "TRANSACTION", "TOKEN_WAIT", "COMMIT", "TRAN_W_TOKEN", "ADJUST_CLOCK", "FORK", "COND_SIG", "COND_WAIT", "COND_WOKE_UP" };

// We are using a circular double linklist to manage those alive threads.
class determ {
private:
   
  // Different status of one thread.
  enum threadStatus {
      STATUS_COND_WAITING = 0, STATUS_BARR_WAITING, STATUS_READY, STATUS_EXIT, STATUS_JOINING, STATUS_OTHERS_NEED_TO_WAIT, STATUS_WAITING_ON_LOCK, STATUS_ON_DECK, STATUS_FORKING
  };

  struct debugging_events{
      //beginning and ending times, given in microseconds from start of program
      unsigned long begin_time_us;
      unsigned long end_time_us;
      int event_type;
      unsigned long long begin_clock;
      unsigned long long end_clock;
      int dirty_pages;
      int updated_pages;
      int partial_pages;
      int merged_pages;
      void * sync_object;
      int coarsening_counter;
      int coarsening_level;
      uint64_t perf_counter_last;
      uint64_t perf_counter_current;
      int cpu;
      uint64_t period_sets;
  };

  class EventEntry {
  public:
      EventEntry(){
          this->event_counter=0;
      }

      void start_event(int type, struct timespec * init_time, void * sync_object) {
#ifdef EVENT_VIEWER
          struct timespec t1;
          clock_gettime(CLOCK_REALTIME, &t1);
          if (this->event_counter < MAX_EVENTS){
              this->events[this->event_counter].event_type=type;
              this->events[this->event_counter].begin_time_us=time_util_time_diff(init_time, &t1);
              this->events[this->event_counter].begin_clock=determ_task_clock_read();
              this->events[this->event_counter].sync_object=sync_object;
          }
#endif
      }
      
      void end_event(int type, struct timespec * init_time, int threadindex){
#ifdef EVENT_VIEWER
          struct timespec t1;
          unsigned numa_node, cpu;
          syscall(__NR_getcpu, &cpu, &numa_node, NULL);

          clock_gettime(CLOCK_REALTIME, &t1);
          if (this->event_counter < MAX_EVENTS){
              //if we don't have a begin event, just use the end of the last event
              if (this->events[this->event_counter].begin_time_us==0 && 
                  this->events[this->event_counter].begin_clock==0 &&
                  this->event_counter > 0){
                      this->events[this->event_counter].event_type=type;
                      this->events[this->event_counter].begin_time_us=this->events[this->event_counter-1].end_time_us;
                      this->events[this->event_counter].begin_clock=this->events[this->event_counter-1].end_clock;
              }

              this->events[this->event_counter].end_time_us=time_util_time_diff(init_time, &t1);
              this->events[this->event_counter].end_clock=determ_task_clock_read();
              this->events[this->event_counter].coarsening_counter=determ_task_clock_get_coarsened_ticks();
              this->events[this->event_counter].perf_counter_last=determ_task_clock_last_raw_perf();
              this->events[this->event_counter].perf_counter_current=determ_task_clock_current_raw_perf();
              this->events[this->event_counter].cpu=cpu;
              this->events[this->event_counter].period_sets=determ_task_clock_period_sets();              
              ++this->event_counter;
          }        
#endif
      }
      
      void add_event_commit_stats(int updated_pages, int merged_pages, int partial_updated_pages, int dirty_pages){
#ifdef EVENT_VIEWER
          if (this->event_counter < MAX_EVENTS && this->events[this->event_counter].event_type==DEBUG_TYPE_COMMIT){
              this->events[this->event_counter].updated_pages=updated_pages;
              this->events[this->event_counter].partial_pages=partial_updated_pages;
              this->events[this->event_counter].dirty_pages=dirty_pages;
              this->events[this->event_counter].merged_pages=merged_pages;
          }
#endif
      }
      
      void add_atomic_event(int type, struct timespec * init_time, void * sync_object){
#ifdef EVENT_VIEWER
          struct timespec t1;
        clock_gettime(CLOCK_REALTIME, &t1);
        if (this->event_counter < MAX_EVENTS){
            this->events[this->event_counter].event_type=type;
            this->events[this->event_counter].begin_time_us=time_util_time_diff(init_time, &t1);
            this->events[this->event_counter].end_time_us=time_util_time_diff(init_time, &t1);
            this->events[this->event_counter].begin_clock=determ_task_clock_read();
            this->events[this->event_counter].end_clock=determ_task_clock_read();
            this->events[this->event_counter].sync_object=sync_object;
            ++this->event_counter;
        }        
#endif
      }


      void add_coarsening_stats(int coarsening_counter, int coarsening_level, uint64_t perf_counter){
#ifdef EVENT_VIEWER
          if (this->event_counter < MAX_EVENTS){
              this->events[this->event_counter].coarsening_counter=coarsening_counter;
              this->events[this->event_counter].coarsening_level=coarsening_level;

          }
#endif          
      }


      void print_last_n_events(int threadindex, int start, int n){
#ifdef EVENT_VIEWER
          cout << "counter: " << this->event_counter << endl;
          for (int i=start;i<start+n;++i){
              cout << "EVENT: " << threadindex << " " << this->events[i].begin_time_us 
                   << " " << this->events[i].end_time_us << " " << this->events[i].event_type << " " 
                   << this->events[i].begin_clock << " " << this->events[i].end_clock - this->events[i].begin_clock << " "
                   << this->events[i].dirty_pages << " " << this->events[i].updated_pages
                   << " " << this->events[i].partial_pages << " " << this->events[i].merged_pages << " " 
                   << this->events[i].sync_object << " " << this->events[i].coarsening_counter << " " 
                   << this->events[i].coarsening_level << " " << this->events[i].perf_counter_current << " " 
                   << this->events[i].perf_counter_last << " " << this->events[i].cpu << " " << determ_task_clock_period_sets() << endl;
          }
#endif
      }

      void print_all_events(int threadindex){
#ifdef EVENT_VIEWER
          cout << "counter: " << this->event_counter << endl;
          for (int i=0;i<this->event_counter;++i){
              cout << "EVENT: " << threadindex << " " << this->events[i].begin_time_us 
                   << " " << this->events[i].end_time_us << " " << this->events[i].event_type << " " 
                   << this->events[i].begin_clock << " " << this->events[i].end_clock - this->events[i].begin_clock << " "
                   << this->events[i].dirty_pages << " " << this->events[i].updated_pages
                   << " " << this->events[i].partial_pages << " " << this->events[i].merged_pages << " " 
                   << this->events[i].sync_object << " " << this->events[i].coarsening_counter << " " 
                   << this->events[i].coarsening_level << " " << this->events[i].perf_counter_current << " " 
                   << this->events[i].perf_counter_last << " " << this->events[i].cpu << " " << determ_task_clock_period_sets() << endl;
          }
#endif
      }
  private:
      int event_counter;
      struct debugging_events events[MAX_EVENTS];
  };

  // Each thread has a thread entry in the system, it is used to control the thread. 
  // For example, when one thread is cond_wait, the corresponding thread entry will be taken out of the token
  // queue and putted into corresponding conditional variable queue.
  class ThreadEntry {
  public:
    inline ThreadEntry() {
      WRAP(pthread_condattr_init)(&this->_condattr);
      pthread_condattr_setpshared(&this->_condattr, PTHREAD_PROCESS_SHARED);
      WRAP(pthread_cond_init)(&this->cond_thread, &this->_condattr);
    }

    inline ThreadEntry(int tid, int threadindex) {
      this->tid = tid;
      this->threadindex = threadindex;
      this->wait = 0;
      WRAP(pthread_condattr_init)(&this->_condattr);
      pthread_condattr_setpshared(&this->_condattr, PTHREAD_PROCESS_SHARED);
      WRAP(pthread_cond_init)(&this->cond_thread, &this->_condattr);
      
    }

    Entry * prev;
    Entry * next;
    volatile int tid; // pid of this thread.
    volatile int threadindex; // thread index 
    volatile int status;
    int tid_parent; // parent's pid
    void * cond; 
    void * barrier;
    size_t wait;
    int joinee_thread_index;
    pthread_cond_t token_cond;
    pthread_mutex_t token_mutex;
    pthread_cond_t cond_thread;
    pthread_condattr_t _condattr;
    int event_counter;
    struct debugging_events events[5000];
    struct timespec last_token_release;
    unsigned long last_logical_clock;
  };

  class SyncVarEntry{
  public:
      int id;
  };

  class LockEntry : SyncVarEntry {
    public:
      // Status of lock, aquired or not.
      volatile bool is_acquired;
      //how many threads are waiting on this lock?
      size_t waiters;
      //waiting queue
      Entry * head;
      pthread_cond_t cond;
  };
  

  // condition variable entry
  class CondEntry : SyncVarEntry {
  public:
    size_t waiters; // How many waiters on this cond.
    void * cond;    // original cond address
    pthread_cond_t realcond;
    Entry * head;   // pointing to the waiting queue
  };

  // barrier entry
  class BarrierEntry : SyncVarEntry {
   public:
     volatile size_t maxthreads;
     volatile size_t threads;
     volatile bool arrival_phase;
     pthread_barrier_t real_barr;
     Entry * head;
     volatile int heap_version;
     volatile int globals_version;
     uint16_t counter;
     uint16_t heapVersion;
     uint16_t globalsVersion;
     uint32_t total_dirty;
     volatile unsigned long committed;
   };

  // Shared mutex and condition variable for all threads.
  // They are useful to synchronize among all threads.
  pthread_mutex_t _mutex;
  //we use this mutex when no NO_DETERM_SYNC is enabled.
  pthread_mutex_t _no_sync_token;
  pthread_cond_t cond;
  pthread_condattr_t _condattr;
  pthread_mutexattr_t _mutexattr;
  pthread_mutexattr_t _mutexattr_tmp;

  volatile size_t _barrierActive;

  // When one thread is created, it will wait until all threads are created.
  // The following two flag are used to indentify whether one thread can move on or not.
  volatile bool _childregistered;
  volatile bool _parentnotified;

  // Some conditional variable used for thread creation and joining.
  pthread_cond_t _cond_children;
  pthread_cond_t _cond_parent;
  pthread_cond_t _cond_join;

  // All threads should be putted into this active list.

  // Currently, we can support how many threads.
  // When one thread is exited, the thread index can be reclaimed.
  ThreadEntry _entries[2048];

  EventEntry _event_entries[1024];

  struct sched_obj{
      int id;
      uint64_t logical_clock;
      int polled;
      int wait_time;
  };

  struct arrival_obj{
      int id;
      uint64_t logical_clock;
      uint64_t real_clock;
  };

  int _waiting_child_threads[MAX_THREADS];
  int _waiting_child_count;

  // how much active thread in the system.
  size_t _maxthreadentries;

  // How many conditional variables in this system.
  // In fact, maybe we don't need this.
  size_t _condnum;
  size_t _barriernum;

  size_t _coresNumb;
  
  //some optimizations for barriers may race, this helps avoid this
  size_t _barrierUpdaters;

  // Variables related to token pass and fence control
  volatile ThreadEntry *_tokenpos;
  volatile size_t _maxthreads;
  volatile size_t _currthreads;
  volatile bool _is_arrival_phase;
  volatile size_t _alivethreads;
  volatile size_t _total_wait_fence_num;
  //when one thread activates another, threads that think they are next
  //in line may no longer be. we set this flag when ever we activate someone
  volatile u_int64_t _activation_counter;

  commit_stats * fence_wait_stats;

  unsigned long total_time;
  unsigned long total_commit_time;
  unsigned long total_wait_time;
  unsigned long commits;
  unsigned long fence_wait_time;

  //last token value
  volatile u_int64_t _last_token_value;
  volatile int _last_putter;
  volatile int _last_getter;
  struct timespec init_time;

  volatile bool master_thread_finished;

  volatile int variable_counter;

 determ():
  _condnum(0),
      _barriernum(0),
      _barrierUpdaters(0),
      total_time(0),
      total_commit_time(0),
      total_wait_time(0),
      fence_wait_time(0),
      commits(0),
      _maxthreads(0),
      _total_wait_fence_num(0),
      _waiting_child_count(0),
      _currthreads(0),
      _is_arrival_phase(false),
      _alivethreads(0),
      _maxthreadentries(MAX_THREADS),
      _tokenpos(NULL), 
      _activation_counter(0),
      _parentnotified(false), 
      _childregistered(false),
      _barrierActive(0),
      master_thread_finished(false),
      variable_counter(0)
          {  }
  
public:

    

  void initialize(void) {
    // Get cores number
    _coresNumb = sysconf(_SC_NPROCESSORS_ONLN);
    if(_coresNumb < 1) {
      fprintf(stderr, "cores number isnot correct. Exit now.\n");
      exit(-1);
    }
    
    // Set up with a shared attribute.
    WRAP(pthread_mutexattr_init)(&_mutexattr);
    pthread_mutexattr_setpshared(&_mutexattr, PTHREAD_PROCESS_SHARED);
    WRAP(pthread_mutexattr_init)(&_mutexattr_tmp);
    pthread_mutexattr_setpshared(&_mutexattr_tmp, PTHREAD_PROCESS_SHARED);

    WRAP(pthread_condattr_init)(&_condattr);
    pthread_condattr_setpshared(&_condattr, PTHREAD_PROCESS_SHARED);


    // Initialize the mutex.
    WRAP(pthread_mutex_init)(&_mutex, &_mutexattr);
    WRAP(pthread_mutex_init)(&_no_sync_token, &_mutexattr);

    WRAP(pthread_cond_init)(&cond, &_condattr);
    WRAP(pthread_cond_init)(&_cond_parent, &_condattr);
    WRAP(pthread_cond_init)(&_cond_children, &_condattr);
    WRAP(pthread_cond_init)(&_cond_join, &_condattr);
    //setup stats stuff
    fence_wait_stats = new commit_stats();
    
    clock_gettime(CLOCK_REALTIME, &init_time);

  }

  static determ& getInstance(void) {
    static determ * determObject = NULL;
    if(!determObject) {
      void *buf = mmap(NULL, sizeof(determ), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      determObject = new(buf) determ();
    }
    return * determObject;
  }

  void __error(int threadindex){
      EventEntry * entry = &_event_entries[threadindex];
      entry->print_all_events(threadindex);
      assert(false);
  }

  void finalize() {
      master_thread_finished=true;
      //print the events for the "original" thread
      _event_entries[0].print_all_events(0);
      WRAP(pthread_cond_destroy)(&cond);
      assert(_currthreads == 0);
  }

  bool is_master_thread_finisehd(){
      return master_thread_finished;
  }

  void print_total_commit_time(){
    cout << "total commit time: " << total_commit_time << " " << commits << endl;
    cout << "total wait: " << total_wait_time << endl;
  }


  void add_total_wait_time(unsigned long usecs){
    total_wait_time+=usecs;
  }

  void add_total_commit_time(unsigned long usecs){
    total_commit_time+=usecs;
    commits++;
  }

  void add_total_time(unsigned long usecs){
    total_time+=usecs;
  }

  // Decrease the fence when one thread exits.
  // We assume that the gobal lock is held now.
  void decrFence(void) {
    _maxthreads--;
    if (_currthreads >= _maxthreads) {
      // Change phase if necessary
      if (_is_arrival_phase && _maxthreads != 0) {
        _is_arrival_phase = false;
        __asm__ __volatile__ ("mfence");
      }
      WRAP(pthread_cond_broadcast)(&cond);
    }
  }

  // pthread_cancel implementation, we are relying threadindex to find corresponding entry.
  bool cancel(int threadindex) {
      ThreadEntry * entry;
      bool isFound = false;
      
      entry = (ThreadEntry *) &_entries[threadindex];
      
      // Checking corresponding status.
      switch (entry->status) {
      case STATUS_EXIT:
          // If the thread has exited, do nothing.
          isFound = false;
          break;
          
      case STATUS_COND_WAITING: 
          // If the thread is waiting on condition variable, remove it from corresponding list. 
          {
              CondEntry * condentry = (CondEntry *) entry->cond;
              removeEntry((Entry *) entry, &condentry->head);
              assert(condentry->waiters == 0);
              assert(condentry->head != NULL);
              isFound = true;
          }
          break;
          
      case STATUS_BARR_WAITING: 
      default:
          // In fact, this case is almost impossible. But just in case, we put code here.
          assert(0);
          isFound = false;
          break;
      }
      
      if (isFound) {
          freeThreadEntry(entry);
      }
      
      //no matter what, print the thread's events
      print_all_thread_events(threadindex);

      // If we can't find the entry, that means this thread has exited successfully.
      // Then we don't need to do anything.
      return isFound;
  }
  
  // There is only one thread which are doing those synchronizations.
  // Hence, there is no need to do those transaction start and end operations.
  bool isSingleWorkingThread(void) {
    return (_maxthreads == 1);
  }

  // All threads are exited now, there is only one thread left in the system.
  bool isSingleAliveThread(void) {
    return (_maxthreads == 1 && _alivethreads == 1);
  }

  inline void add_event_commit_stats(int threadindex, int updated_pages, int merged_pages, int partial_updated_pages, int dirty_pages){        
#ifdef EVENT_VIEWER
      EventEntry * entry = &_event_entries[threadindex];
      entry->add_event_commit_stats(updated_pages, merged_pages, partial_updated_pages, dirty_pages);
#endif
  }

  inline void start_thread_event(int threadindex, int event_type, void * sync_object){
#ifdef EVENT_VIEWER
      EventEntry * entry = &_event_entries[threadindex];
      entry->start_event(event_type, &init_time, sync_object);
#endif
  }

  inline void end_thread_event(int threadindex, int event_type){
#ifdef EVENT_VIEWER
      EventEntry * entry = &_event_entries[threadindex];
      entry->end_event(event_type, &init_time, threadindex);
#endif
  }

  void add_atomic_event(int threadindex, int event_type, void * sync_object){
#ifdef EVENT_VIEWER
      EventEntry * entry = &_event_entries[threadindex];
      entry->add_atomic_event(event_type, &init_time, sync_object);
#endif
  }


  void add_coarsening_stats(int threadindex, int coarsening_counter, int coarsening_level, bool successful, uint64_t perf_counter){
#ifdef EVENT_VIEWER
      start_thread_event(threadindex, (successful) ? DEBUG_TYPE_TX_COARSE_SUCCESS : DEBUG_TYPE_TX_COARSE_FAILED, NULL);
      EventEntry * entry = &_event_entries[threadindex];
      entry->add_coarsening_stats(coarsening_counter, coarsening_level, perf_counter);
      end_thread_event(threadindex, DEBUG_TYPE_TX_COARSE_FAILED);
#endif          
      }


  void print_all_thread_events(int threadindex){
#ifdef EVENT_VIEWER
      EventEntry * entry = &_event_entries[threadindex];
      lock();
      entry->print_all_events(threadindex);
      unlock();
#endif
  }


  inline void commitAndUpdate(){
      xmemory::commit();
  }

  bool isTokenHolder(int threadindex){
      ThreadEntry * entry = &_entries[threadindex];
      return ((ThreadEntry *)_tokenpos == entry);
  }

  // main function of waitFence and waitToken
  // Here, we are defining two different paths.
  // If the alive threads is smaller than the coresNumb, then we don't
  // use those condwait, but busy waiting instead.
  void waitFence(int threadindex, bool keepBitmap) {
    int rv;
    bool lastThread = false;
    return;
  }
  
  int getTokenFromBarrier(int threadindex, void * barr){
      __getToken(threadindex, barr);
  }

  int getToken(int threadindex){
      __getToken(threadindex, NULL);
  }

  int __getToken(int threadindex, void * barrierObj) {
    int cond_result;
    ThreadEntry * entry = &_entries[threadindex];
    struct timespec t1, t2;

    start_thread_event(threadindex, DEBUG_TYPE_WAIT_LOWEST, NULL);
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "GET TOKEN: begin wait, " << threadindex << " id " << determ_task_get_id() << " clock " << determ_task_clock_read() << " pid " << getpid() << endl;
#endif

#ifdef NO_DETERM_SYNC

    end_thread_event(threadindex, DEBUG_TYPE_WAIT_LOWEST);
    
    xmemory::partialUpdate();
    
  getToken:
    int counter=0;
    //need to wait here until the token has been released
    while (_tokenpos != NULL) {
#ifdef OPT_UPDATES_WHILE_WAITING
        xmemory::partialUpdate();
#endif
        Pause();
    }        

    if (!__sync_bool_compare_and_swap ((size_t *)(&_tokenpos), NULL, (size_t)entry )) {
        goto getToken;
    }


#else
  startover:
    int spin_counter=0;
    int polled=0;
    uint64_t local_activation_counter=_activation_counter;
    int is_lowest=0;

    while(!is_lowest && spin_counter<10000000){
        is_lowest=determ_task_clock_is_lowest();
        local_activation_counter=_activation_counter;
        xmemory::partialUpdate();
        ++spin_counter;
    }

    if (!is_lowest){
        //need to potentially poll
        polled=determ_task_clock_is_lowest_wait();
    }
    
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "GET TOKEN: We are the lowest, " << threadindex << " id " << determ_task_get_id() << " clock " << determ_task_clock_read() << " pid " << getpid() << " polled " << polled << endl;
#endif

    pid_t pid = getpid();

    end_thread_event(threadindex, DEBUG_TYPE_WAIT_LOWEST);

    start_thread_event(threadindex, DEBUG_TYPE_TOKEN_WAIT, NULL);
  getToken:
    int counter=0;
    //need to wait here until the token has been released
    while (_tokenpos != NULL) {
        Pause();
    }        

    if (!__sync_bool_compare_and_swap ((size_t *)(&_tokenpos), NULL, (size_t)entry )) {
        goto getToken;
    }

    if (_activation_counter!=local_activation_counter){
        determ_task_clock_on_wakeup();
        //we need to order these writes
        __asm__ __volatile__ ("mfence");
        //setting the _tokenpos to NULL will "release" it
        _tokenpos=NULL;
        add_atomic_event(threadindex, DEBUG_TYPE_TOKEN_FAILED, NULL);
        goto startover;
        }
#endif


    _last_getter=threadindex;


    end_thread_event(threadindex, DEBUG_TYPE_TOKEN_WAIT);

#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "GET TOKEN: acquired token, " << threadindex << " clock " << determ_task_clock_read() << endl;
#endif

    return 0;
  }

  void putToken(int threadindex){
      __putToken(threadindex, true, true);
  }

  void putTokenLockHeld(int threadindex){
      __putToken(threadindex, false, true);
  }

  void putTokenNoFastForward(int threadindex){
      __putToken(threadindex, true, false);
  }

  void __attribute((optimize(0))) __putToken(int threadindex, bool acquireLocks, bool considerForFastForward) {
    ThreadEntry * next;
    ThreadEntry * entry = &_entries[threadindex];

    STOP_TIMER(serial);
    
    if (acquireLocks)
        lock();

    if (_tokenpos==NULL){
        __error(threadindex);
    }

    // Sanity check, whether I have to right to call putToken.
    // Only token owner can put token.
    if (threadindex != _tokenpos->threadindex) {
      unlock();
      fprintf(stderr, "%d : ERROR to putToken, pointing to pid %d index %d, while my index %d\n", getpid(), _tokenpos->tid, _tokenpos->threadindex, threadindex);
      assert(0);
    }
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "PUT TOKEN: releasing token, " << _tokenpos->threadindex << endl;
#endif
    //for some operations, we don't want to count
    if (considerForFastForward){
        _last_token_value=determ_task_clock_read();
    }

    //we need to order these writes
    __asm__ __volatile__ ("mfence");
    //setting the _tokenpos to NULL will "release" it
    _tokenpos=NULL;
    _last_putter=threadindex;

    if (acquireLocks)
        unlock();
    
    clock_gettime(CLOCK_REALTIME, &entry->last_token_release);
    entry->last_logical_clock=determ_task_clock_read();

#ifdef NO_DETERM_SYNC
    WRAP(pthread_mutex_unlock)(&_no_sync_token);
#endif


  }

  int getLastTokenPutter(){
      return _last_putter;
  }

  int getLastTokenHolder(){
      return _last_getter;
  }

  u_int64_t getLastTokenClock(){
      return _last_token_value;
  }

  // No need lock since the register is done before any spawning.
  void registerMaster(int threadindex, int pid) {
      registerThread(threadindex, pid, 0);
      _maxthreads = 1;
      _alivethreads = 1;
      _is_arrival_phase = true; 
  }

  // Add this thread to the list.
  // When one thread is registered, no one else is running,
  // there is no need to hold the lock.
  void registerThread(int threadindex, int pid, int parentindex) {
    ThreadEntry * entry;

    //cout << "thread " << threadindex << " is pid " << pid << " clock " << determ_task_clock_read() << endl;

    // Allocate memory to hold corresponding information.
    void * ptr = allocThreadEntry(threadindex);

    // Here, header is not one node in the circular link list.
    // _activelist->next is the first node in the link list, while _activelist->prev is
    // the last node in the link list.
    entry = new (ptr) ThreadEntry(pid, threadindex);

    // fprintf(stderr, "%d: with threadindex %d\n", getpid(), threadindex);

    // Record the parent's thread index.
    entry->tid_parent = parentindex;

    // Add this thread to the list.
    //if (_tokenpos == NULL) {
    //_tokenpos = entry;
    //}

    entry->status = STATUS_READY;
    
    WRAP(pthread_mutex_init)(&entry->token_mutex, &_mutexattr);
    WRAP(pthread_cond_init)(&entry->token_cond, &_condattr);

    // Add one entry according to their threadindex.
    //insertTail((Entry *)entry, &_activelist);
  }

  inline bool join(int guestindex, int myindex, bool wakeup) {
    // Check whether I am holding the lock or not.
    assert (myindex == _tokenpos->threadindex);

    struct timespec t1;
    ThreadEntry * joinee;
    ThreadEntry * myentry;
    ThreadEntry * wakeupEntry;
    bool toWaitToken = false;
    bool halted = false;

    lock();

    // Get next entry.
    myentry = (ThreadEntry *)&_entries[myindex];
    joinee = (ThreadEntry *)&_entries[guestindex];

    // When the joinee is still alive, we should wait for the joinee to wake me up 
    if(joinee->status != STATUS_EXIT) {
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "JOIN: removing from consideration, " << myindex << "guest index " << guestindex << endl;
#endif
        // Remove myself from consideration for the token
        determ_task_clock_halt();
        halted=true;

        // Set my status to joinning.
        myentry->status = STATUS_JOINING;
        myentry->joinee_thread_index = guestindex;
    }
  
    while(joinee->status != STATUS_EXIT) {    
        //we need this here, because the child that wakes us up will be waiting for this
        //signal to be either set back to STATUS_JOINING or STATUS_READY
        myentry->status = STATUS_JOINING;
        //we need to release the token if we have it
        if (_tokenpos && _tokenpos->threadindex==myindex){
#ifdef DTHREADS_TASKCLOCK_DEBUG
            cout << "PUT TOKEN: releasing token in join, " << myindex << endl;
#endif
            putTokenLockHeld(myindex);
        }
        // Waiting for the children's exit now.
        WRAP(pthread_cond_wait)(&_cond_join, &_mutex);
#ifdef DTHREADS_TASKCLOCK_DEBUG
        cout << "JOIN: joinee exited and joiner woke up, " << myindex << endl;
#endif
        toWaitToken = true;
    }
    
    if (halted){
        determ_task_clock_activate();
    }
    __asm__ __volatile__ ("mfence");
    //its important that this comes *after* the activation. The reason is because
    //we can't have a race to get the token next after the parent has woken up. The
    //parent needs to at least activate first and get back "in the running" before
    //the child gives up the token. 
    myentry->status = STATUS_READY;

    unlock(); 
    if(toWaitToken) {
        getToken(myindex);
        START_TIMER(serial);
    }

    return toWaitToken;
  }


  void starting_fork(int threadindex){
      ((ThreadEntry *)&_entries[threadindex])->status=STATUS_FORKING;   
  }

  void deregisterThread(int threadindex) {
    ThreadEntry * entry = &_entries[threadindex];
    ThreadEntry * parent = &_entries[entry->tid_parent];
    ThreadEntry * nextentry;
    EventEntry * evententry = &_event_entries[threadindex];

    lock();
    DEBUG("%d: Deregistering", getpid());
    //fence_wait_stats->stats_pid_print_time("waitFence");

    // Decrease number of alive threads and fence.
    if(_alivethreads > 0) {
      // Since this thread is running with the token, no need to modify 
      _alivethreads--;
    }

    // Remove this thread entry from activelist.  
    //removeEntry((Entry *) entry, &_activelist);

    freeThreadEntry(entry);
    
    // Whether the parent is trying to join current thread now??
    if(parent->status == STATUS_JOINING && parent->joinee_thread_index == threadindex) {
        //setting this status so that we wait for the parent to wake up. We'll know this when
        //they either a) set the status to STATUS_JOINING or b) wake up and set the status to READY
        parent->status=STATUS_OTHERS_NEED_TO_WAIT;
        //paranoia, but lets do it anyway
        __asm__ __volatile__ ("mfence");
        // Waken up all threads waiting on _cond_join, but only the one waiting on this thread
        // can be waken up. Other thread will goto sleep again immediately.
        WRAP(pthread_cond_broadcast)(&_cond_join);
        //need to unlock so that the parent can actually wake up.
        unlock();
        //we need to hold on to the token until the parent wakes up...
        while(parent->status==STATUS_OTHERS_NEED_TO_WAIT){
            Pause();
        }
        lock();
    }

    //Now we need to remove ourselves from consideration for being the "lowest"
    determ_task_clock_halt();
    determ_task_clock_reset();
    
#ifdef DTHREADS_TASKCLOCK_DEBUG
    cout << "PUT TOKEN: releasing token in dereg, " << threadindex << endl;
#endif
    putTokenLockHeld(threadindex);

    unlock();
    
  }

  //call this while holding the token
  void wait_on_lock_and_release_token(void * mutex, int threadindex){
      LockEntry * lockentry = (LockEntry *)getSyncEntry(mutex);
      ThreadEntry * entry = &_entries[threadindex];
      start_thread_event(threadindex, DEBUG_TYPE_LOCK_FAILED, mutex);
      lock();
      //add ourselves to the wait list
      insertTail((Entry *)entry, &lockentry->head);
      //increase the number of waiters
      lockentry->waiters++;
      //set our entry status to waiting
      entry->status=STATUS_WAITING_ON_LOCK;
      unlock();
      //Now we need to remove ourselves from consideration for being the "lowest"
      determ_task_clock_halt();
      //release the token
      putToken(threadindex);
      //finish the commit (if necessary)
#ifdef USE_DEFERRED_WORK
      xmemory::commit_deferred_end();
#endif      

  spin:
      //spin for a little before waiting on the cond
      int spinning_counter=0;
      int spinning_max=500000;
      while(entry->status!=STATUS_READY && (spinning_counter++ < spinning_max)){
          Pause();
      }
      //if spinning didn't work...lets actually go to sleep
      if (entry->status!=STATUS_READY){
          lock();
          while(entry->status!=STATUS_READY){
              WRAP(pthread_cond_wait)(&entry->cond_thread, &_mutex);
              if (entry->status==STATUS_ON_DECK){
                  unlock();
                  goto spin;
              }
          }
          add_atomic_event(threadindex, DEBUG_TYPE_LOCK_CONDVAR_WAKE, NULL);
          unlock();
      }
      determ_task_clock_on_wakeup();
      end_thread_event(threadindex, DEBUG_TYPE_LOCK_FAILED);
      assert(entry->status==STATUS_READY);
  }

  LockEntry * lock_init(void * mutex) {
      //    fprintf(stderr, "%d: lockinit with mutex %p\n", getpid(), mutex);
      LockEntry * entry = allocLockEntry();
      //No one acquire the lock in the beginning.
      entry->is_acquired = false;
      // No one is the owner.
      setSyncEntry(mutex, (void *)entry); 
      entry->head=NULL;
      pthread_condattr_setpshared(&_condattr, PTHREAD_PROCESS_SHARED);
      WRAP(pthread_cond_init)(&entry->cond, &_condattr);
      return entry;
  }
  
  void lock_destroy(void * mutex) {
      LockEntry * entry =(LockEntry*)getSyncEntry(mutex);
      clearSyncEntry(mutex);
      freeSyncEntry(entry);
  }
  
  //must be called while holding the token
  inline bool lock_acquire(void * mutex, int threadindex) {
    LockEntry * entry = (LockEntry *)getSyncEntry(mutex);
    if(entry == NULL) {
        //commit and update, in case someone already initialized the lock
        commitAndUpdate();
        if ((entry=(LockEntry *)getSyncEntry(mutex))==NULL){
            entry = lock_init(mutex);
            //its safe to do this here because we're holding the token, or we're in single threaded mode
            commitAndUpdate();
        }
    }
    if(entry->is_acquired == true)  {
        return false;
    }
    else{
        entry->is_acquired = true;
        //add_atomic_event(threadindex, DEBUG_TYPE_MUTEX_LOCK, mutex);
        return true;
    }
  }

  inline int lock_waiters_count(void * mutex){
    LockEntry * entry = (LockEntry *)getSyncEntry(mutex);
    int count=0;
    lock();
    count = entry->waiters;
    unlock();
    return count;
  }

  //must be called while holding the token
  inline void lock_release(void * mutex, int threadindex) {
    LockEntry * entry = (LockEntry *)getSyncEntry(mutex);
    lock();
    //add_atomic_event(threadindex, DEBUG_TYPE_MUTEX_UNLOCK, mutex);
    entry->is_acquired = false;
    if (entry->waiters>0){
        //need to notify the next thread
        ThreadEntry * waitingThread = (ThreadEntry *)removeHeadEntry(&entry->head);
        ThreadEntry * onDeckThread=NULL;
        entry->waiters--;
        if (determ_task_clock_activate_other(waitingThread->threadindex)){
            //cout << "activated " << waitingThread->threadindex << " new low " << endl;
            _activation_counter++;
        }
        waitingThread->status=STATUS_READY;
        if (entry->waiters > 0){
            //still people waiting...lets wake up the next guy so he's ready
            onDeckThread = (ThreadEntry *)getHeadEntry(&entry->head);
            if (onDeckThread!=NULL){
                onDeckThread->status=STATUS_ON_DECK;
            }
        }
        unlock();
        WRAP(pthread_cond_signal)(&waitingThread->cond_thread);
        if (onDeckThread!=NULL){
            WRAP(pthread_cond_signal)(&onDeckThread->cond_thread);
        }
        //cout << "  Woke up " << waitingThread->threadindex << endl;
    }
    else{
        unlock();
    }
  }

  CondEntry * cond_get_entry_and_check(void * user_cond) {assert(false);}

  CondEntry * cond_init(void * cond) {
      CondEntry * entry = allocCondEntry();
      entry->waiters=0;
      entry->head=NULL;
      entry->cond=cond;
      setSyncEntry(cond, (void *)entry);
      
      pthread_condattr_setpshared(&_condattr, PTHREAD_PROCESS_SHARED);
      WRAP(pthread_cond_init)(&entry->realcond, &_condattr);
      return entry;
  }

  void cond_destroy(void * cond) {assert(false);}

  void cond_wait(int threadindex, void * user_cond, void * thelock) {
      //get the thread entry
      ThreadEntry * entry = &_entries[threadindex];
      //get the cond entry
      CondEntry * condentry = (CondEntry*)getSyncEntry(user_cond);
      //printEntries(&condentry->head);
      //insert our entry into the list of waiting threads
      insertTail((Entry *)entry, &condentry->head);
      //another waiter for the cond
      condentry->waiters++;
      //set the thread to be waiting
      entry->cond=condentry;
      entry->status=STATUS_COND_WAITING;
      //release the lock
      lock_release(thelock, threadindex);
      lock();
      //take ourselves out of the running for the token
      determ_task_clock_halt();
      //release the token
      putTokenLockHeld(threadindex);
      start_thread_event(threadindex, DEBUG_TYPE_WAIT_ON_COND, user_cond);
      //wait on the "real" cond entry
      while(entry->status!=STATUS_READY){
          WRAP(pthread_cond_wait)(&condentry->realcond, &_mutex);
      }
      unlock();
      end_thread_event(threadindex, DEBUG_TYPE_WAIT_ON_COND);
      determ_task_clock_on_wakeup();
  }

  // Current thread are going to send out signal.
  void cond_signal(int threadindex, void * user_cond) {
      //get the thread entry
      ThreadEntry * entry = &_entries[threadindex];
      //get the cond entry
      CondEntry * condentry = (CondEntry*)getSyncEntry(user_cond);
      assert(condentry->waiters>=0);
      //we have the token, so its safe to check if there are any waiters
      if (condentry->waiters==0){
          return;
      }
      //we have some work to do
      lock();
      //get the entry to signal
      ThreadEntry * waitingThread = (ThreadEntry *)removeHeadEntry(&condentry->head);
      assert(waitingThread != NULL);
      waitingThread->cond=NULL;
      waitingThread->status=STATUS_READY;
      condentry->waiters--;
      //activate this thread to ensure determinism
      if (determ_task_clock_activate_other(waitingThread->threadindex)){
          //cout << "activated " << waitingThread->threadindex << " new low " << endl;
          _activation_counter++;
      }
      //we have to wake everyone up...since there's no way to target just the thread that has been chosen
      int result=WRAP(pthread_cond_broadcast)(&condentry->realcond);
      unlock();

  }

  void cond_broadcast(int threadindex, void * user_cond) {
      //get the thread entry
      ThreadEntry * entry = &_entries[threadindex];
      //get the cond entry
      CondEntry * condentry = (CondEntry*)getSyncEntry(user_cond);
      assert(condentry->waiters>=0);
      //we have the token, so its safe to check if there are any waiters
      if (condentry->waiters==0){
          return;
      }
      //we have some work to do
      lock();
      //get the entry to signal
      ThreadEntry * waitingThread; 
      while((waitingThread=(ThreadEntry *)removeHeadEntry(&condentry->head))!=NULL){
          waitingThread->cond=NULL;
          waitingThread->status=STATUS_READY;
          condentry->waiters--;
          //activate this thread to ensure determinism
          determ_task_clock_activate_other(waitingThread->threadindex);
      }
      //we have to use our index here, since we're waking up so many threads
      _activation_counter++;
      //we have to wake everyone up...since there's no way to target just the thread that has been chosen
      int result=WRAP(pthread_cond_broadcast)(&condentry->realcond);
      unlock();

  }
    
  int sig_wait(const sigset_t *set, int *sig, int threadindex) {assert(false);}

  // Functions related to barrier.
  void barrier_init(void * bar, int count) {

      BarrierEntry * entry = allocBarrierEntry();
      pthread_barrierattr_t attr;
      
      if (entry == NULL) {
          assert(0);
      }
      
      //a bunch of bookkeeping stuff...some may not be needed anymore
      entry->maxthreads = count;
      entry->threads = 0;
      entry->arrival_phase = true;
      entry->head = NULL;
      entry->counter=0;
      entry->heapVersion=0;
      entry->globalsVersion=0;
      entry->committed=0;
      entry->total_dirty=0;

      pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
      WRAP(pthread_barrier_init)(&entry->real_barr, &attr, count);

      setSyncEntry((void *)bar, (void *)entry);
  }

  int __find_order(uint8_t order_array[], int threadindex, int maxthreads){
      int seen=0;
      for (int i=0;(i<MAX_THREADS) && (seen < maxthreads);i++){
          if (i==threadindex){
              return seen;
          }
          if (order_array[i]){
              seen++;
          }
      }
      assert(false);
  }

#ifdef USE_SIMPLE_BARRIER
  void barrier_wait(void * b, int threadindex){
      int halted=0;
      BarrierEntry * barr = (BarrierEntry *)getSyncEntry(b);
      getToken(threadindex);
      start_thread_event(threadindex, DEBUG_TYPE_COMMIT, b);
      commitAndUpdate();
      end_thread_event(threadindex, DEBUG_TYPE_COMMIT);
      start_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT, b);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
      if (++barr->counter<barr->maxthreads){
          //remove ourselves so others can make progress
          determ_task_clock_halt();
          putToken(threadindex);
          halted=1;
      }
      //wait on the real barrier
      WRAP(pthread_barrier_wait)(&barr->real_barr);
      if (halted){
          //lets wake up
          determ_task_clock_activate();
          //TODO: REALLY???
          determ_task_clock_on_wakeup();
      }
      //wait for everyone to wake up
      WRAP(pthread_barrier_wait)(&barr->real_barr);
      end_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT);
      if (halted){
          getToken(threadindex);
      }
      else{
          barr->counter=0;
      }
      start_thread_event(threadindex, DEBUG_TYPE_COMMIT, b);
      commitAndUpdate();
      end_thread_event(threadindex, DEBUG_TYPE_COMMIT);
#ifdef TOKEN_ORDER_ROUND_ROBIN
      determ_task_clock_add_ticks(LOGICAL_CLOCK_ROUND_ROBIN_INFINITY);
#endif
      putToken(threadindex);
  }

#else

  void __wait_for_commiters(BarrierEntry * barr, unsigned long heapVersion, unsigned long globalsVersion){
      int boundedWait=100000;
      int counter=0;
      while(counter<boundedWait && barr->committed<(barr->maxthreads-1)){
          counter++;
          if (barr->committed > barr->maxthreads/2 && barr->total_dirty<barr->maxthreads*10){
              break;
          }
          else{
              if (xmemory::get_current_heap_version() > heapVersion ||
                  xmemory::get_current_globals_version() > globalsVersion){
                  xmemory::update();
                  counter=0;
              }
          }
      }
  }

  void barrier_wait(void * b, int threadindex) {
      BarrierEntry * barr = (BarrierEntry *)getSyncEntry(b);
      //we are committing in parallel, but may need to wait for previous versions to 
      //finish committing...these variables will track which version to wait for.
      unsigned long heapVersion, globalsVersion, ourHeapVersion, ourGlobalsVersion;
      struct timespec t1,t2,t3,t4;
      //get the token
      getToken(threadindex);
      fflush(stdout);
      //no other threads other than those arriving on this barrier can get the token
      _barrierActive=(size_t)b;
      //increment the counter to mark our arrival
      barr->counter++;
      //what versions will we be waiting for when we perform the commit?
      if (barr->counter==1){
          //first thread through sets the base version numbers to be equal to their version
          heapVersion=barr->heapVersion=xmemory::get_current_heap_version();
          globalsVersion=barr->globalsVersion=xmemory::get_current_globals_version();
      }
      else{
          heapVersion=barr->heapVersion;
          globalsVersion=barr->globalsVersion;
      }

      barr->total_dirty+=xmemory::get_dirty_pages();

      //figure out our version to wait for heap and globals.
      //Only add one if we are going to create a new version on this segment
      barr->heapVersion+=(xmemory::get_dirty_pages_heap()>0) ? 1 : 0;
      ourHeapVersion=barr->heapVersion;
      barr->globalsVersion+=(xmemory::get_dirty_pages_globals()>0) ? 1 : 0;
      ourGlobalsVersion=barr->globalsVersion;
      //all threads except for the last one to arrive need to remove themselves from contention
      //for the token, and release the lock
      if(barr->counter!=barr->maxthreads){
          determ_task_clock_halt();
          putToken(threadindex);
      }
      //perform the commit, in parallel, with all your friends!!!
      start_thread_event(threadindex, DEBUG_TYPE_COMMIT, b);
      //just count dirty pages for now
      add_event_commit_stats(threadindex, xmemory::get_updated_pages(), xmemory::get_merged_pages(), 0, xmemory::get_dirty_pages());
      xmemory::commit_parallel(heapVersion, globalsVersion);
      xatomic::increment(&barr->committed);
      end_thread_event(threadindex, DEBUG_TYPE_COMMIT);
      start_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT, b);
      __wait_for_commiters(barr,ourHeapVersion, ourGlobalsVersion);
      //wait for all our friends to finish their commit
      WRAP(pthread_barrier_wait)(&barr->real_barr);
      end_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT);
      //add yourself back...one of us holds the token so nothing can go wrong
      if (!isTokenHolder(threadindex)){
          determ_task_clock_activate();
      }
      //if it got to the point where there was only a single active thread...just clear that flag
      determ_task_clock_clear_single_active_thread();
      start_thread_event(threadindex, DEBUG_TYPE_COMMIT, b);
      //everyone has committed...but earlier threads still need to update to see the newer stuff
      xmemory::update();
      end_thread_event(threadindex, DEBUG_TYPE_COMMIT);
      start_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT, b);
      //wait for the update to finish
      WRAP(pthread_barrier_wait)(&barr->real_barr);
      end_thread_event(threadindex, DEBUG_TYPE_BARRIER_WAIT);
      //everyone is done, the token holder just needs to clean up
      if(isTokenHolder(threadindex)){
          _barrierActive=0;
          barr->counter=0;
          barr->heapVersion=0;
          barr->globalsVersion=0;
          barr->committed=0;
          barr->total_dirty=0;
          //we activated a bunch of guys...we need to do this
          _activation_counter++;
          putToken(threadindex);
      }
  }

#endif

  void barrier_destroy(void * bar) {assert(false);}

  int get_syncvar_id(void * var){
      SyncVarEntry * syncVar = (SyncVarEntry *)getSyncEntry(var);
      return syncVar->id;
  }

private:
  inline void * allocThreadEntry(int threadindex) {
    assert(threadindex < _maxthreadentries);
    return (&_entries[threadindex]);
  }

  inline void freeThreadEntry(void *ptr) {
    ThreadEntry * entry = (ThreadEntry *) ptr;
    entry->status = STATUS_EXIT;
    // Do nothing now.
    return;
  }
  
  inline void * allocSyncEntry(int size) {
      SyncVarEntry * syncEntry = (SyncVarEntry *)InternalHeap::getInstance().malloc(size);
#ifdef PRINT_SCHEDULE
      syncEntry->id=variable_counter++;
#endif
    return syncEntry;
  }

  inline void freeSyncEntry(void * ptr) {
      if (ptr != NULL) {
          InternalHeap::getInstance().free(ptr);
      }
  }

  inline LockEntry *allocLockEntry(void) {
    //fprintf(stderr, "%d: alloc lock entry with size %d\n", getpid(), sizeof(LockEntry));  
    return ((LockEntry *) allocSyncEntry(sizeof(LockEntry)));
  }

  inline CondEntry *allocCondEntry(void) {
    return ((CondEntry *) allocSyncEntry(sizeof(CondEntry)));
  }

  inline BarrierEntry *allocBarrierEntry(void) {
    return ((BarrierEntry *) allocSyncEntry(sizeof(BarrierEntry)));
  }

  void * getSyncEntry(void * entry) {
    return(*((void **)entry));
  }

  void setSyncEntry(void * origentry, void * newentry) {
      *((size_t *)origentry)=(size_t)newentry;
  }

  void clearSyncEntry(void * origentry) {
    void **dest = (void**)origentry;

    *dest = NULL;

    // Update the shared copy in the same time. 
    xmemory::mem_write(*dest, NULL);
  }
  inline void lock(void) {
    WRAP(pthread_mutex_lock)(&_mutex);
  }

  inline void unlock(void) {
    WRAP(pthread_mutex_unlock)(&_mutex);
  }
};

#endif
