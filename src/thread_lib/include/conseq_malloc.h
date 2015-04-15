
#include "xmemory.h"

class conseq_malloc{

 public:
    
    static inline void * malloc(size_t sz){
        char * tmp = NULL;
        void * ptr = xmemory::malloc(sz);
        return ptr;        
    }
    
    static inline void * calloc(size_t nmemb, size_t sz) {
        void * ptr = xmemory::malloc(nmemb * sz);
        memset(ptr, 0, nmemb * sz);
        return ptr;
    }
    
    static inline void free(void * ptr) {
        xmemory::free(ptr);
    }
    
    static inline size_t getSize(void * ptr) {
        return xmemory::getSize(ptr);
    }
    
    static inline void * realloc(void * ptr, size_t sz) {
        void * newptr;
        if (ptr == NULL) {
            newptr = xmemory::malloc(sz);
            return newptr;
        }
        if (sz == 0) {
            xmemory::free(ptr);
            return NULL;
        }
        
        newptr = xmemory::realloc(ptr, sz);
        return newptr;
    }
};
