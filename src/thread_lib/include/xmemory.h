// -*- C++ -*-
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
 * @file   xmemory.h
 * @brief  Manage different kinds of memory, main entry for memory.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 * @author Tim Merrifield <http://www.cs.uic.edu/Bits/TimothyMerrifield>
 */

#ifndef _XMEMORY_H_
#define _XMEMORY_H_

#include <signal.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <set>

#include "xglobals.h"

// Heap Layers
#include "heaplayers/stlallocator.h"
#include "warpheap.h"

#include "xoneheap.h"
#include "xheap.h"

#include "xpageentry.h"
#include "objectheader.h"
#include "internalheap.h"
// Encapsulates all memory spaces (globals & heap).

#define CONV_REVERT 512

#if __x86_64__
/* 64-bit */
#define __CONV_SYS_CALL 303

#else

#define __CONV_SYS_CALL 341

#endif

class xmemory {
private:
  /// The globals region.
  static xglobals _globals;

  /// Protected heap.
  static warpheap<xdefines::NUM_HEAPS, xdefines::PROTECTEDHEAP_CHUNK, xoneheap<xheap<PROTECTEDHEAP_SIZE> > > _pheap;

  /// A signal stack, for catching signals.
  static stack_t _sigstk;

  static int _heapid;

  // Private on purpose
  xmemory(void) {}

public:

  static void initialize(void) {
    DEBUG("initializing xmemory");
    // Call _pheap so that xheap.h can be initialized at first and then can work normally.
    _pheap.initialize();
    _globals.initialize();
    xpageentry::getInstance().initialize();

    // Initialize the internal heap.
    InternalHeap::getInstance().initialize(); 
  }

  static void finalize(void) {
    _globals.finalize();
    _pheap.finalize();
  }

  static void setThreadIndex(int id) {
    _globals.setThreadIndex(id);
    _pheap.setThreadIndex(id);

    // Calculate the sub-heapid by the global thread index.
    _heapid = id % xdefines::NUM_HEAPS;
  }

  static inline void *malloc(size_t sz) {
    void * ptr = _pheap.malloc(_heapid, sz);
    return ptr;
  }

  static inline void * realloc(void * ptr, size_t sz) {
    size_t s = getSize(ptr);
    void * newptr = malloc(sz);
    if (newptr) {
      size_t copySz = (s < sz) ? s : sz;
      memcpy(newptr, ptr, copySz);
    }
    free(ptr);
    return newptr;
  }

  static inline void free(void * ptr) {
    return _pheap.free(_heapid, ptr);
  }

  /// @return the allocated size of a dynamically-allocated object.
  static inline size_t getSize(void * ptr) {
    // Just pass the pointer along to the heap.
    return _pheap.getSize(ptr);
  }

  static void clearUserInfo(void){
    _globals.clearUserInfo();
    _pheap.clearUserInfo();
  }

  static void openProtection(void) {
    _globals.openProtection(NULL);
    _pheap.openProtection(_pheap.getend());
  }

  static void closeProtection(void) {
    _globals.closeProtection();
    _pheap.closeProtection();
  }

  static inline void begin(bool cleanup) {
    // Reset global and heap protection.
    _globals.begin(cleanup);
    _pheap.begin(cleanup);
  }

    static inline bool inHeapRange(void * addr){
        return _pheap.inRange(addr);
    }

    static inline bool inGlobalsRange(void * addr){
        return _globals.inRange(addr);
    }

  static void mem_write(void * dest, void *val) {
    if(_pheap.inRange(dest)) {
      _pheap.mem_write(dest, val);
    } else if(_globals.inRange(dest)) {
      _globals.mem_write(dest, val);
    }
  }

  static inline void handleWrite(void * addr) {
    
  }

    static inline void update(){
        _pheap.update();
        _globals.update();
    }

    static inline void partialUpdate(){
        _pheap.partialUpdate();
        _globals.partialUpdate();
    }

    static inline int get_dirty_pages(){
        return _pheap.get_dirty_pages() + _globals.get_dirty_pages();
    }

    static inline int get_dirty_pages_heap(){
        return _pheap.get_dirty_pages();
    }

    static inline int get_dirty_pages_globals(){
        return _globals.get_dirty_pages();
    }

    static inline int get_updated_pages(){
        return (_pheap.get_updated_pages() + _globals.get_updated_pages());
    }

    static inline int get_merged_pages(){
        return _pheap.get_merged_pages() + _globals.get_merged_pages();
    }
    
    static inline int get_partial_unique_pages(){
        return _pheap.get_partial_unique_pages() + _globals.get_partial_unique_pages();
    }

    static inline int get_logical_pages(){
        return _pheap.get_logical_pages() + _globals.get_logical_pages();
    }

    static inline void * get_heap_start(){
        return _pheap.get_segment_start();
    }


    
    static inline int sleep(){
        _pheap.sleep();
        _globals.sleep();
    }

    static inline int wake(){
        _pheap.wake();
        _globals.wake();
    }


    static inline void checkpoint(){
        //cout << "checkpoint " << getpid() << endl;
        _pheap.checkpoint();
        _globals.checkpoint();
    }

    static inline void commit() {
        //cout << "commit " << getpid() << endl;
        _pheap.checkandcommit();
        _globals.checkandcommit();
    }

    static inline void commit_deferred_start(){
        _pheap.commit_deferred_start();
        _globals.commit_deferred_start();
    }

    static inline void commit_deferred_end(){
        _pheap.commit_deferred_end();
        _globals.commit_deferred_end();
    }


    static inline void commit_parallel(uint64_t heapVersionToWaitFor, uint64_t globalsVersionToWaitFor){
        //cout << "commit " << getpid() << endl;
        _pheap.commit_parallel(heapVersionToWaitFor);
        _globals.commit_parallel(globalsVersionToWaitFor);
    }

    static inline int get_current_heap_version(){
        return _pheap.get_current_version();
    }

    static inline int get_current_globals_version(){
        return _globals.get_current_version();
    }

    static inline int get_local_heap_version(){
        return _pheap.get_local_version();
    }
    
    static inline int get_local_globals_version(){
        return _globals.get_local_version();
    }
    
    static inline void set_local_version_tag(unsigned int tag){
        _pheap.set_local_version_tag(tag);
        _globals.set_local_version_tag(tag);
    }

    static unsigned long long __rdtsc(void)
    {
        unsigned long low, high;
        asm volatile("rdtsc" : "=a" (low), "=d" (high));
        return ((low) | (high) << 32);
    }

    //revert heap and globals
    static void __attribute__ ((noinline)) revert_heap_and_globals(){
        unsigned long long start, end;
        int initialDirtyPages, postRevertDirty;

        initialDirtyPages = _pheap.get_dirty_pages() + 
           _globals.get_dirty_pages();
        
        start = __rdtsc();
        _pheap.revert(_heapid);
        _pheap.revert_heap_and_globals();
        _globals.revert_heap_and_globals();
        postRevertDirty = _pheap.get_dirty_pages() + 
           _globals.get_dirty_pages();
        //cycles, non-checkpointed pages, checkpointed, total
        cout << "revert_heap_and_globals: " << __rdtsc() - start 
             << " " << initialDirtyPages - postRevertDirty << " " 
             << postRevertDirty << " " << initialDirtyPages << endl;
    }

    static inline void begin_speculation(){
        _pheap.begin_speculation(_heapid);
    }

    static inline void end_speculation(){
        _pheap.end_speculation(_heapid);
    }
};

#endif
//08048710q

// asmlinkage long sys_conversion_sync(unsigned long start, int flags, size_t editing_distance);
