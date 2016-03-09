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
 * @file xdefines.h
 * @brief Some definitions which maybe modified in the future.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 */

#ifndef _XDEFINES_H_
#define _XDEFINES_H_
#include <sys/types.h>
#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "prof.h"

typedef struct runtime_data {
  volatile unsigned long thread_index;
  struct runtime_stats stats;
} runtime_data_t;

extern runtime_data_t *global_data;

#define Pause()        asm volatile("rep; nop" ::: "memory")

#define PERSIST_COMMIT_NORMAL 0
#define PERSIST_COMMIT_MERGE_ONLY 2
#define PERSIST_COMMIT_UPDATE_ONLY 3

#define PROTECTEDHEAP_SIZE (1048576UL * 20000)

#define MAX_THREADS 64

class xdefines {
public:
    enum { STACK_SIZE = (1048576UL * 100) } ; // 1 * 1048576 };
  enum { PROTECTEDHEAP_CHUNK = 10485760 };
  
  enum { MAX_GLOBALS_SIZE = 1048576UL * 2000 };
  enum { INTERNALHEAP_SIZE = 1048576UL * 20000 }; // FIXME 10M 
  enum { PageSize = 4096UL };
  enum { PAGE_SIZE_MASK = (PageSize-1) };
  enum { NUM_HEAPS = 128 };
  enum { LOCK_OWNER_BUDGET = 10 };

};


//taken from gcc documentation
#define xmax(a,b)               \
    ({ __typeof__ (a) _a = (a);   \
        __typeof__ (b) _b = (b);    \
        _a > _b ? _a : _b; })


//taken from gcc documentation
#define xmin(a,b)               \
    ({ __typeof__ (a) _a = (a);     \
        __typeof__ (b) _b = (b);    \
        _a < _b ? _a : _b; })

#endif
