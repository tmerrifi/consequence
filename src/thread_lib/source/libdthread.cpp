// -*- C++ -*-

/*
 Author: Emery Berger, http://www.cs.umass.edu/~emery
 
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
 * @file   xrun.h
 * @brief  The main engine for consistency management, etc.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Tim Merrifield <http://www.cs.uic.edu/Bits/TimothyMerrifield>
 */

#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "debug.h"
#include "prof.h"

#include "xrun.h"

#define _GNU_SOURCE

#if defined(__GNUG__)
void initialize() __attribute__((constructor));
void finalize() __attribute__((destructor));
#endif

runtime_data_t *global_data;

static bool initialized = false;

void initialize() {
  DEBUG("intializing libdthread");
  
  init_real_functions();
  
  global_data = (runtime_data_t*)mmap(NULL, xdefines::PageSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  
  global_data->thread_index = 1;
  DEBUG("after mapping global data structure");
  xrun::initialize();
  initialized = true;
}

void finalize() {
	DEBUG("finalizing libdthread");
	initialized = false;
	xrun::done();
	//fprintf(stderr, "\nStatistics information:\n");
	//PRINT_TIMER(serial);
	PRINT_COUNTER(commit);
	//PRINT_COUNTER(twinpage);
	//PRINT_COUNTER(suspectpage);
	//PRINT_COUNTER(slowpage);
	//PRINT_COUNTER(dirtypage);
	//PRINT_COUNTER(lazypage);
	//PRINT_COUNTER(shorttrans);
}

extern "C" {

void * malloc(size_t sz) {
	void * ptr;

	if (!initialized) {
		DEBUG("Pre-initialization malloc call forwarded to mmap");
		ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	} else {
		ptr = xrun::malloc(sz);
	}
	if (ptr == NULL) {
		fprintf(stderr, "%d: Out of memory!\n", getpid());
		::abort();
	}
	return ptr;
}

void * calloc(size_t nmemb, size_t sz) {
	void * ptr;

        if (nmemb==0){
            return NULL;
        }
        else if (sz==0){
            //this is silly but ferret from Parsec 2.1 relies on this undefined behavior
            sz=1;
        }

	if (!initialized) {
		DEBUG("Pre-initialization calloc call forwarded to mmap");
		ptr = mmap(NULL, sz * nmemb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		memset(ptr, 0, sz * nmemb);
	} else {
		ptr = xrun::calloc(nmemb, sz);
	}

	if (ptr == NULL) {
		fprintf(stderr, "%d: Out of memory!\n", getpid());
		::abort();
	}

	memset(ptr, 0, sz * nmemb);

	return ptr;
}

void free(void * ptr) {
	//assert(initialized);
	if(initialized) {
		xrun::free(ptr);
	} else {
		DEBUG("Pre-initialization free call ignored");
	}
}

void * memalign(size_t boundary, size_t size) {
    size_t counter, boundary_tmp;
    //ensure boundary is a power of 2
    for (counter=0, boundary_tmp=boundary; boundary_tmp; counter++){
        boundary_tmp&=(boundary_tmp - 1);
    }
    if (counter==1){
        //we don't actually have time to support memalign for real, so instead
        //we are going to allocate the requested size plus some extra space to
        //move over if needed
        size_t ptr = (size_t)malloc(size + (boundary * 2));
        //do we have work to do?
        if (ptr & (boundary-1)){
            ptr=(ptr & (~(boundary-1))) + boundary;
        }
        return (void *)ptr;
    }
    
    return NULL;
}

size_t malloc_usable_size(void * ptr) {
	//assert(initialized);
	if(initialized) {
		return xrun::getSize(ptr);
	} else {
		DEBUG("Pre-initialization malloc_usable_size call ignored");
	}
	return 0;
}

void * realloc(void * ptr, size_t sz) {
	//assert(initialized);
	if(initialized) {
		return xrun::realloc(ptr, sz);
	} else {
		DEBUG("Pre-initialization realloc call ignored");
	}
	return NULL;
}

int getpid(void) {
	if(initialized) {
		return xrun::id();
	}
	return 0;
}

void pthread_exit(void * value_ptr) {
    cout << "ERROR: EXIT NOT IMPLEMENTED" << endl;
    assert(0);
}

int pthread_cancel(pthread_t thread) {
    if(initialized) {
        xrun::cancel((void*) thread);
    }
    return 0;
}

int pthread_setconcurrency(int) {
	return 0;
}

int pthread_attr_init(pthread_attr_t *) {
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *) {
	return 0;
}

pthread_t pthread_self(void) {
	if(initialized) {
		return (pthread_t)xrun::id();
	}
	return 0;
}

int pthread_kill(pthread_t thread, int sig) {
    cout << "ERROR: KILL NOT IMPLEMENTED" << endl;
    assert(0);
}

int sigwait(const sigset_t *set, int *sig) {
    cout << "ERROR: SIGWAIT NOT IMPLEMENTED" << endl;
    assert(0);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *) {
	if(initialized) {
		return xrun::mutex_init(mutex);
	}
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	if(initialized) {
		xrun::mutex_lock(mutex);
	}
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	DEBUG("pthread_mutex_trylock is not supported");
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	if(initialized) {
		xrun::mutex_unlock(mutex);
		//printf("done!!! %d\n", getpid());
	}
	return 0;
}

int pthread_spin_init(pthread_spinlock_t *lock, int pshared){
    pthread_mutex_init((pthread_mutex_t *)lock, NULL);
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock){
    pthread_mutex_lock((pthread_mutex_t *)lock);
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock){
    pthread_mutex_unlock((pthread_mutex_t *)lock);
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock){
    //for now just return
    return 0;
}
    
int pthread_mutex_destory(pthread_mutex_t *mutex) {
	if(initialized) {
		return xrun::mutex_destroy(mutex);
	}
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *, size_t * s) {
    cout << "ERROR: getstacksize NOT IMPLEMENTED" << endl;
    assert(0);
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *) {
	return 0;
}
int pthread_mutexattr_init(pthread_mutexattr_t *) {
	return 0;
}
int pthread_mutexattr_settype(pthread_mutexattr_t *, int) {
	return 0;
}
int pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *) {
	return 0;
}
int pthread_attr_setstacksize(pthread_attr_t *, size_t) {
	return 0;
}



    
int pthread_create (pthread_t * tid, const pthread_attr_t * attr, void *(*fn) (void *), void * arg) {
    if(initialized) {
        xrun::spawn(fn, arg, tid);
    }
    return 0;
}

int pthread_join(pthread_t tid, void ** val) {
	//assert(initialized);
	if(initialized) {
		xrun::join((void*)tid, val);
	}
	return 0;
}

int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t *attr) {
    xrun::cond_init(cond);
}

int pthread_cond_broadcast(pthread_cond_t * cond) {
    xrun::cond_broadcast(cond);
}

int pthread_cond_signal(pthread_cond_t * cond) {
    xrun::cond_signal(cond);
}

int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    return xrun::cond_wait(cond,mutex);
}

int pthread_cond_destroy(pthread_cond_t * cond) {
    xrun::cond_destroy(cond);
}

// Add support for barrier functions
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t * attr, unsigned int count) {
	//assert(initialized);
	if(initialized) {
		return xrun::barrier_init(barrier, count);
	}
	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
	//assert(initialized);
	if(initialized) {
		return xrun::barrier_destroy(barrier);
	}
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
	//assert(initialized);
	if(initialized) {
		return xrun::barrier_wait(barrier);
	}
	return 0;
}

ssize_t write(int fd, const void *buf, size_t count) {
	uint8_t *start = (uint8_t*)buf;
	volatile int temp; 
	
	for(size_t i=0; i<count; i += xdefines::PageSize) {
		temp = start[i];
	}

	temp = start[count-1];
	
	return WRAP(write)(fd, buf, count);
}
	
ssize_t read(int fd, void *buf, size_t count) {
	uint8_t *start = (uint8_t*)buf;
	
	for(size_t i=0; i<count; i += xdefines::PageSize) {
		start[i] = 0;
	}
	
	start[count-1] = 0;
	
	return WRAP(read)(fd, buf, count);
}

    int close (int fd){
        xrun::beginSysCall();
        int result=WRAP(close)(fd);
        xrun::endSysCall();
        return result;
    }

    int __open_2(const char * pathname, int flags){
        xrun::beginSysCall();
        int result=WRAP(__open_2)(pathname, flags);
        xrun::endSysCall();
        return result;
    }

    /*int creat(const char * pathname, mode_t mode){
        xrun::beginSysCall();        
        int result=WRAP(creat)(pathname, mode);
        xrun::endSysCall();
        return result;
        }*/


    
    void perror ( const char * str ){
        cout << str << " " << getpid() << endl;
    }

    int nanosleep(const struct timespec * rec, struct timespec * rem){
        cout << "in nano sleep..." << endl;
        xrun::beginSysCallDeactivate();
        int result=WRAP(nanosleep)(rec, rem);
        xrun::endSysCallActivate();
        return result;
    }

    unsigned int sleep(unsigned int secs){
        xrun::beginSysCallDeactivate();
        int result=WRAP(sleep)(secs);
        xrun::endSysCallActivate();
        return result;
    }

    
    void * __conseq_mmap(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    
        if (initialized){
            xrun::beginSysCall();
        }
        void * result = (void *)syscall(SYS_mmap, (unsigned long)addr, (unsigned long)length,
                                            (unsigned long)prot, (unsigned long)flags,
                                            (unsigned long) fd, (unsigned long)offset);
        if (initialized){
            xrun::endSysCall();
        }

        return result;
    }


    void * mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
        return __conseq_mmap(addr, length, prot, flags, fd, offset);
    }
    
    void * mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
        return __conseq_mmap(addr, length, prot, flags, fd, offset);
    }

    int sched_yield(void){
        if (initialized){
            xrun::schedYield();
        }
        return 0;
    }
    
}
