// -*- C++ -*-
#ifndef _XPERSIST_H_
#define _XPERSIST_H_
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
 * @file   xpersist.h
 * @brief  Main file to handle page fault, commits.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 * @author Tim Merrifield <http://www.cs.uic.edu/Bits/TimothyMerrifield>
 */


#include <set>
#include <list>
#include <vector>
#include <map>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#include <ksnap.h>
#include <cv_determ.h>

#include "xatomic.h"
#include "heaplayers/ansiwrapper.h"
#include "heaplayers/freelistheap.h"

#include "heaplayers/stlallocator.h"
#include "privateheap.h"
#include "xdefines.h"
#include "xbitmap.h"

#include "debug.h"
#include "xpageentry.h"
#include "stats.h"
#include "time_util.h"

/**
 * @class xpersist
 * @brief Makes a range of memory persistent and consistent.
 */
template<class Type, unsigned long NElts = 1>
class xpersist {
public:

  /// @arg startaddr  the optional starting address of the local memory.
  xpersist(void * startaddr = 0, size_t startsize = 0) :
    _startaddr(startaddr), _startsize(startsize) {

    // Check predefined globals size is large enough or not. 
    if (_startsize > 0) {
      if (_startsize > NElts * sizeof(Type)) {
        fprintf(stderr, "This persistent region (%Zd) is too small (%Zd).\n", NElts * sizeof(Type), _startsize);
        ::abort();
      }
    }

    //for global vars, we need to copy the contents somewhere, and then back once we mmap
    //the memory segment.
    Type * tmp;
    if (startaddr != NULL){
      tmp = (Type *)malloc(startsize);
      memcpy(tmp, (Type *)startaddr, startsize);
    }

    if ((_startaddr=mmap((startaddr)?startaddr:NULL,
             NElts * sizeof(Type),
             PROT_READ|PROT_WRITE,
                         ((startaddr)?MAP_FIXED:0)|MAP_SHARED|MAP_ANONYMOUS,-1,0)) == NULL){
      fprintf(stderr, "conversion open failed somehow.\n");
      ::abort();        
    }

    if (startaddr && tmp){
      memcpy((Type *)startaddr,tmp, startsize);
    }

    isSpeculating = false;
  }

  void clearUserInfo(void){}

  void initialize(void) {}

    void closeProtection(void){}

  void finalize(void) {}

    void openProtection(void * end) {
        _isProtected = true;
        _trans = 0;
    }

    int get_current_version(){
        return 0;
    }

    int get_local_version(){
        return 0;
    }

    int get_dirty_pages(){
        return 0;
    }

    int get_updated_pages(){
        return 0;
    }

    int get_merged_pages(){
        return 0;
    }

    int get_logical_pages(){
        return 0;
    }
    
    int get_partial_unique_pages(){
        return 0;
    }

    void revert_heap_and_globals(){
        //NOP
    }


  void setThreadIndex(int index) {
    _threadindex = index;
  }

  /// @return true iff the address is in this space.
  inline bool inRange(void * addr) {
    if (((size_t) addr >= (size_t) base()) && ((size_t) addr
        < (size_t) base() + size())) {
      return true;
    } else {
      return false;
    }
  }

  /// @return the start of the memory region being managed.
  inline Type * base(void) const {
    return (Type *)_startaddr;
  }

  /// @return the size in bytes of the underlying object.
  inline unsigned long size(void) const {
    return NElts * sizeof(Type);
  }
  
  bool nop(void) {}

  /// @brief Start a transaction.
  inline void begin(bool cleanup) {}

  inline void recordPageChanges(int pageNo) {}

  // Get the start address of specified page.
  inline void * getPageStart(int pageNo) {
    return ((void *)((intptr_t)base() + pageNo * xdefines::PageSize));
  }

  inline void merge_for_barrier(){}

  inline void update_for_barrier(){}

  inline void settle_for_barrier(){}

  inline void update(){}
    
  inline void checkandcommit() {}

  inline void checkpoint(){}
    
    inline void commit_parallel(int versionToWaitFor){}

    /// @brief Commit all writes.
    inline void memoryBarrier() {}

    inline void partialUpdate(){}

    inline void sleep(){}

    inline void wake(){}

    inline void commit_deferred_start(){}

    inline void commit_deferred_end(){}

    inline void set_local_version_tag(unsigned int tag){}

    inline void * get_segment_start(){
        return _startaddr;
    }

  bool mem_write(void * addr, void *val) {
    unsigned long offset = (intptr_t)addr - (intptr_t)base();
    void **ptr = (void**)((intptr_t)_startaddr + offset);
    *ptr = val;
    //fprintf(stderr, "addr %p val %p(int %d) with ptr %p\n", addr, val, (unsigned long)val, *ptr);
    return true;
  }


private:

    bool isSpeculating;
    
    /// True if current xpersist.h is a heap.
    bool _isHeap;
    
    /// The starting address of the region.
    void * _startaddr;
    
    /// The size of the region.
    const size_t _startsize;
    
    /// The file descriptor for the backing store.
    int _backingFd;
    
    bool _isProtected;
    
    unsigned int _trans;
    
    /// The length of the version array.
    enum {  TotalPageNums = NElts * sizeof(Type) / (xdefines::PageSize) };

    int _threadindex;
};

#endif
