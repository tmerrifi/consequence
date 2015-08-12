#ifndef CONSEQ_SYNC_TYPES_H
#define CONSEQ_SYNC_TYPES_H

#include "xmemory.h"
#include "list.h"
#include "syncstats.h"

class SyncVarEntry{
 public:
    int id;
    uint64_t last_committed;
    syncStats * stats;
};

class LockEntry :public SyncVarEntry {
 public:
    // Status of lock, aquired or not.
    volatile bool is_acquired;
    //how many threads are waiting on this lock?
    size_t waiters;
    //waiting queue
    Entry * head;
    pthread_cond_t cond;
    uint16_t owner;
};


// condition variable entry
class CondEntry : public SyncVarEntry {
 public:
    size_t waiters; // How many waiters on this cond.
    void * cond;    // original cond address
    pthread_cond_t realcond;
    Entry * head;   // pointing to the waiting queue
};

// barrier entry
class BarrierEntry : public SyncVarEntry {
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

inline void * allocSyncEntry(int size, int counter) {
      SyncVarEntry * syncEntry = (SyncVarEntry *)InternalHeap::getInstance().malloc(size);
      syncEntry->last_committed=0;
      void * statsMem=xmemory::malloc(sizeof(syncStats));
      syncEntry->stats = new (statsMem) syncStats();
#ifdef PRINT_SCHEDULE
      syncEntry->id=counter;
#endif
    return syncEntry;
  }

  inline void freeSyncEntry(void * ptr) {
      if (ptr != NULL) {
          InternalHeap::getInstance().free(ptr);
      }
  }

  inline void * getSyncEntry(void * entry) {
    return(*((void **)entry));
  }

  inline void setSyncEntry(void * origentry, void * newentry) {
      *((size_t *)origentry)=(size_t)newentry;
  }

  inline void clearSyncEntry(void * origentry) {
    void **dest = (void**)origentry;

    *dest = NULL;

    // Update the shared copy in the same time. 
    xmemory::mem_write(*dest, NULL);
  }



#endif
