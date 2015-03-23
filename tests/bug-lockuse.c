#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#ifndef THREADS
#define THREADS 8
#endif

#define PAGE_SIZE (1<<12)
#define PAGES 20
#define NUM_OF_PAGES (PAGES - (PAGES % THREADS))
#define NUM_OF_BYTES (PAGE_SIZE * NUM_OF_PAGES)
#define ITERATIONS 150

uint8_t mem[NUM_OF_BYTES];
int counter=0;

int failures=0;

/* Global lock */
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void do_work(size_t id){
    for (int i=0;i<NUM_OF_PAGES;++i){
        mem[(i*PAGE_SIZE)+id]++;
    }
}

int count_array(){
    int count=0;
    for (int i=0;i<NUM_OF_BYTES;++i){
        count+=mem[i];
    }
    return count;
}

void validate(int counter){
    int count = counter*NUM_OF_PAGES;
    int real_count = count_array();
    if (count!=real_count){
        failures++;
        printf("failed, count: %d, real_count: %d, counter: %d\n", count, real_count, counter);
    }
}


void * child_thread(void * data)
{
    int id=*((int *)data);
    for (int i=0;i<ITERATIONS;++i){
        do_work(id);
        pthread_mutex_lock(&g_lock);
        /* Do 1ms computation work. */
        ++counter;
        /* Page access */
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
} 


int main(int argc,char**argv)
{
	pthread_t waiters[THREADS];
	int thread_id[THREADS];
	int i;
        memset(mem, 0, NUM_OF_BYTES);
	for(i = 0; i < THREADS; i++){
	  thread_id[i]=i;
	  pthread_create (&waiters[i], NULL, child_thread, (void *)&(thread_id[i]));
	}

	for(i = 0; i < THREADS; i++)
		pthread_join (waiters[i], NULL);

        validate(counter);

        if (counter==ITERATIONS*THREADS && failures==0){
            fprintf(stderr, "lockuse: SUCCEEDED\n");
        }
        else{
            fprintf(stderr, "lockuse: FAILED, failures %d, counter %d expected %d\n", failures, counter, ITERATIONS*THREADS);
        }
        
	return 0;
}
