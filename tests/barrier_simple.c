#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

//how many threads to try this on
#ifndef THREADS
#define THREADS 4
#endif

#define PAGE_SIZE (1<<12)
#define NUM_OF_PAGES (500 - (500 % THREADS))
#define NUM_OF_BYTES (PAGE_SIZE * NUM_OF_PAGES)
//Don't set this to be more than 255...we don't want overflows!
#define ITERATIONS 100

uint8_t mem[NUM_OF_BYTES];

pthread_barrier_t barrier;

//increment a byte (at your id) per page
void do_work(size_t id){
    for (int i=0;i<NUM_OF_PAGES;++i){
        //find "your" byte in the page and increment it
        mem[(i*PAGE_SIZE)+id]++;
    }
}

//count up all the bytes in the array
int count_array(){
    int count=0;
    for (int i=0;i<NUM_OF_BYTES;++i){
        count+=mem[i];
    }
    return count;
}

void validate(int id, int iteration){
    //estimate the count in the current array
    int count = (iteration*NUM_OF_PAGES*THREADS) + NUM_OF_PAGES;
    //count up the "real" array
    int real_count = count_array();
    //if they don't match, we have a problem
    if (count != real_count){
        fprintf(stderr, "FAILURE\n");
    }
}

void * run (void * input){
    //grab the thread id
    size_t id = *((size_t *)input);
    for (int i=0;i<ITERATIONS;++i){
        //increment bytes in memory
        do_work(id);
        //Validate to make sure what is in memory is what is expected
        validate(id,i);
        //wait for our friends
        pthread_barrier_wait(&barrier);
    }
    return NULL;
}

int main(){
    pthread_t threads[THREADS];
    size_t threadIds[THREADS];

    if (ITERATIONS>256-1){
        printf("whoops! ITERATIONS is capped at 255 to avoid overflows\n");
        exit(1);
    }
    
    //initialize our barrier with THREADS # of threads
    pthread_barrier_init(&barrier, NULL, THREADS);

    //create a bunch of threads and call run()
    for (int i=0;i<THREADS;++i){
        threadIds[i]=i;
        pthread_create(&threads[i], NULL, run, &threadIds[i]);
    }

    //join on our threads
    for (int i=0;i<THREADS;++i){
        pthread_join(threads[i], NULL);
    }
    //validate again
    if (count_array()==(ITERATIONS * NUM_OF_PAGES * THREADS)){
        fprintf(stderr, "barrier_simple: SUCCEEDED\n");
    }
    else{
        fprintf(stderr, "barrier_simple: FAILED\n");
    }
}
