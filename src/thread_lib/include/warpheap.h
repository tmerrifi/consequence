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

/*
 * @file   warpheap.h
 * @brief  A heap optimized to reduce the likelihood of false sharing.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */
#ifndef _WARPHEAP_H_
#define _WARPHEAP_H_

#include "xdefines.h"
#include "heaplayers/util/sassert.h"
#include "xadaptheap.h"
#include "real.h"

#include "heaplayers/ansiwrapper.h"
#include "heaplayers/kingsleyheap.h"
#include "heaplayers/adapt.h"
//#include "heaplayers/adaptheap.h"
#include "heaplayers/util/sllist.h"
#include "heaplayers/util/dllist.h"
#include "heaplayers/sanitycheckheap.h"
#include "heaplayers/zoneheap.h"
#include "objectheader.h"

template<class SourceHeap>
class NewSourceHeap: public SourceHeap {
public:
	void * malloc(size_t sz) {

		void * ptr = SourceHeap::malloc(sz + sizeof(objectHeader));
		if (!ptr) {
			return NULL;
		}
		objectHeader * o = new (ptr) objectHeader(sz);
		void * newptr = getPointer(o);

		assert (getSize(newptr) >= sz);
		return newptr;
	}
	void free(void * ptr) {
		SourceHeap::free((void *) getObject(ptr));
	}
	size_t getSize(void * ptr) {
		objectHeader * o = getObject(ptr);
		size_t sz = o->getSize();
		if (sz == 0) {
			fprintf(stderr, "%d : getSize error, with ptr = %p. sz %Zd \n", getpid(), ptr, sz);
		}
		return sz;
	}
private:

	static objectHeader * getObject(void * ptr) {
		objectHeader * o = (objectHeader *) ptr;
		return (o - 1);
	}

	static void * getPointer(objectHeader * o) {
		return (void *) (o + 1);
	}
};

template<class SourceHeap, int chunky>
class KingsleyStyleHeap: public HL::ANSIWrapper<HL::StrictSegHeap<
		Kingsley::NUMBINS, Kingsley::size2Class, Kingsley::class2Size,
		HL::AdaptHeap<HL::SLList, NewSourceHeap<SourceHeap> >, NewSourceHeap<
				HL::ZoneHeap<SourceHeap, chunky> > > > {
private:

	typedef HL::ANSIWrapper<HL::StrictSegHeap<Kingsley::NUMBINS,
			Kingsley::size2Class, Kingsley::class2Size, HL::AdaptHeap<
								      HL::SLList, NewSourceHeap<SourceHeap> >, NewSourceHeap<
														 HL::ZoneHeap<SourceHeap, chunky> > > > SuperHeap;

public:
	KingsleyStyleHeap(void) {
		//     printf ("this kingsley = %p\n", this);
	}

	void * malloc(size_t sz) {
		void * ptr = SuperHeap::malloc(sz);
		return ptr;
	}

private:
	// char buf[4096 - (sizeof(SuperHeap) % 4096) - sizeof(int)];
//  char buf[4096 - (sizeof(SuperHeap) % 4096)];
};

template<int NumHeaps, class TheHeapType>
class PPHeap: public TheHeapType {
public:

	void * malloc(int ind, size_t sz) {
	  // Try to get memory from the local heap first.
	  void * ptr = _heap[ind].malloc(sz);
	  return ptr;
	}

	void free(int ind, void * ptr) {
	  // Put the freed object onto this thread's heap.  Note that this
	  // policy is essentially pure private heaps, (see Berger et
	  // al. ASPLOS 2000), and so suffers from numerous known problems.
	  _heap[ind].free(ptr);
	}

    void begin_speculation(int ind){
        _heap[ind].begin_speculation();
    }

    void end_speculation(int ind){
        _heap[ind].end_speculation();
    }

    void revert(int ind){
        _heap[ind].revert_speculation();
    }

    
	
private:
    //pthread_mutex_t * _lock[NumHeaps];
    TheHeapType _heap[NumHeaps];
};

template<class SourceHeap, int ChunkSize>
class PerThreadHeap: public PPHeap<xdefines::NUM_HEAPS, KingsleyStyleHeap<
		SourceHeap, ChunkSize> > {
};

template<int NumHeaps, int ChunkSize, class SourceHeap>
class warpheap: public xadaptheap<PerThreadHeap, SourceHeap, ChunkSize> {
};

#endif // _WARPHEAP_H_
