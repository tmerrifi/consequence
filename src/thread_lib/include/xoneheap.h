// -*- C++ -*-

/*
 Author: Emery Berger, http://www.cs.umass.edu/~emery
 
 Copyright (c) 2007-8 Emery Berger, University of Massachusetts Amherst.

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

#ifndef _XONEHEAP_H_
#define _XONEHEAP_H_

/**
 * @class xoneheap
 * @brief Wraps a single heap instance.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

template<class SourceHeap>
class xoneheap {
public:
  xoneheap() {
  }

  void initialize(void) {
    getHeap()->initialize();
  }
  void finalize(void) {
    getHeap()->finalize();
  }
  void begin(bool cleanup) {
    getHeap()->begin(cleanup);
  }

#ifdef LAZY_COMMIT
  void finalcommit(bool release) {
    getHeap()->finalcommit(release);
  }

  void forceCommit(int pid, void * end) {
    getHeap()->forceCommitOwnedPages(pid, end);
  }
#endif

    void commit_deferred_start(){
        getHeap()->commit_deferred_start();
    }

    void commit_deferred_end(){
        getHeap()->commit_deferred_end();
    }


  void checkandcommit() {
    getHeap()->checkandcommit();
  }

    void commit_parallel(int expectedVersion){
        getHeap()->commit_parallel(expectedVersion);
    }

    void checkpoint(){
        getHeap()->checkpoint();
    }
    
    void partialUpdate(){
        getHeap()->partialUpdate();
    }

  void clearUserInfo(void){
    getHeap()->clearUserInfo();
  }

  void * getend(void) {
    return getHeap()->getend();
  }

  void stats(void) {
    getHeap()->stats();
  }



    int get_partial_unique_pages(){
        return getHeap()->get_partial_unique_pages();
    }

    int get_dirty_pages(){
        return getHeap()->get_dirty_pages();
    }

    int get_updated_pages(){
        return getHeap()->get_updated_pages();
    }

    int get_merged_pages(){
        return getHeap()->get_merged_pages();
    }

    int get_logical_pages(){
        return getHeap()->get_logical_pages();
    }

    void update(){
        getHeap()->update();
    }

    void sleep(){
        getHeap()->sleep();
    }

    void wake(){
        getHeap()->wake();
    }

    void * get_segment_start(){
        return getHeap()->get_segment_start();
    }
    
    int get_current_version(){
        return getHeap()->get_current_version();
    }

    int get_local_version(){
        return getHeap()->get_local_version();
    }


  void openProtection(void *end) {
    getHeap()->openProtection(end);
  }
  void closeProtection(void) {
    getHeap()->closeProtection();
  }

  bool nop(void) {
    return getHeap()->nop();
  }

  bool inRange(void * ptr) {
    return getHeap()->inRange(ptr);
  }
  void handleWrite(void * ptr) {
    getHeap()->handleWrite(ptr);
  }

  bool mem_write(void * dest, void *val) {
    return getHeap()->mem_write(dest, val);
  }

  void setThreadIndex(int index) {
    return getHeap()->setThreadIndex(index);
  }

    void begin_speculation(){
        getHeap()->begin_speculation();
    }

    void end_speculation(){
        getHeap()->end_speculation();
    }
    
  void * malloc(size_t sz) {
    return getHeap()->malloc(sz);
  }
  void free(void * ptr) {
    getHeap()->free(ptr);
  }
  size_t getSize(void * ptr) {
    return getHeap()->getSize(ptr);
  }

    void set_local_version_tag(unsigned int tag){
        getHeap()->set_local_version_tag(tag);
    }

  void revert_heap_and_globals(){
    getHeap()->revert_heap_and_globals();
  }

private:

  SourceHeap * getHeap(void) {
    static char heapbuf[sizeof(SourceHeap)];
    static SourceHeap * _heap = new (heapbuf) SourceHeap;
    //fprintf (stderr, "heapbuf is %p\n", _heap);
    return _heap;
  }

};

#endif // _XONEHEAP_H_
