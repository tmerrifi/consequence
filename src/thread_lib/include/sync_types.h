#ifndef CONSEQ_SYNC_TYPES_H
#define CONSEQ_SYNC_TYPES_H

#include "xmemory.h"
#include "xdefines.h"
#include "list.h"
#include "syncstats.h"

struct speculationSyncStats{
    unsigned long results[MAX_THREADS];
};

class SyncVarEntry{
public:
   int id;
   uint64_t last_committed;
   uint16_t committed_by;
   struct speculationSyncStats stats;   
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
    //start off perfect
    memset(&syncEntry->stats, 0xFF, sizeof(struct speculationSyncStats));
    //syncEntry->stats = new (statsMem) syncStats();
    syncEntry->id=counter;
    return syncEntry;
  }

  inline void ALWAYS_INLINE freeSyncEntry(void * ptr) {
      if (ptr != NULL) {
          InternalHeap::getInstance().free(ptr);
      }
  }

static inline void * ALWAYS_INLINE getSyncEntry(void * entry) {
  return(*((void **)entry));
}

static inline void ALWAYS_INLINE setSyncEntry(void * origentry, void * newentry) {
      *((size_t *)origentry)=(size_t)newentry;
  }

static inline void ALWAYS_INLINE clearSyncEntry(void * origentry) {
    void **dest = (void**)origentry;
    *dest = NULL;
    // Update the shared copy in the same time. 
    xmemory::mem_write(*dest, NULL);
  }

static inline void ALWAYS_INLINE specStatsSuccess(SyncVarEntry * syncEntry, int tid){
    syncEntry->stats.results[tid]=(syncEntry->stats.results[tid]<<1)|1;
}

static inline void ALWAYS_INLINE specStatsFailed(SyncVarEntry * syncEntry, int tid, int penalty){
    syncEntry->stats.results[tid]=(syncEntry->stats.results[tid]<<penalty);
}

static inline double ALWAYS_INLINE specStatsSuccessRate(SyncVarEntry * syncEntry, int tid){
   #ifdef SPEC_DISABLE_PER_LOCK_STATS
   return 1.0;
   #else
   return ((double)__builtin_popcountl(syncEntry->stats.results[tid]))/64.0;
   #endif
}

static inline int ALWAYS_INLINE specStatsSucceededLastTime(SyncVarEntry * syncEntry, int tid){
    return syncEntry->stats.results[tid] & 0x1UL;
}


#endif
