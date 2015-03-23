#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <sched.h>

#define PAGE_SIZE 4096
#define NUM_OF_PAGES 10
#define ARR_SIZE_IN_BYTES (PAGE_SIZE * NUM_OF_PAGES)

#ifndef LOCK_GRANULARITY_IN_BYTES
#define LOCK_GRANULARITY_IN_BYTES (PAGE_SIZE*1)
#endif

#define NUM_MUTEXES (ARR_SIZE_IN_BYTES/LOCK_GRANULARITY_IN_BYTES)
#define BYTES_PER_OBJECT sizeof(uint32_t)
#define NUM_OBJECTS (ARR_SIZE_IN_BYTES/BYTES_PER_OBJECT)

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif

#ifndef CS_WORK
#define CS_WORK 1
#endif

#define OUTER_ITERATIONS ((1<<7)/NUM_THREADS)
#define INNER_ITERATIONS ((1<<7)/NUM_THREADS)

uint32_t arr[ARR_SIZE_IN_BYTES];

pthread_mutex_t mutexes[NUM_MUTEXES];

int __attribute__((optimize(0))) pause_thread(int pause_count){
    int total=0;
    for(int i=0;i<pause_count;++i){
        total++;
    }
    return total;
}

int get_lock_index(int mem_index){
    return ((mem_index*BYTES_PER_OBJECT)/LOCK_GRANULARITY_IN_BYTES);
}

int sum_array(){
    int sum=0;
    for (int i=0;i<NUM_OBJECTS;++i){
        sum+=arr[i];
    }
    return sum;
}

void * do_work(void * _id){
    printf("%d\n", sched_getcpu());
    int id =* ((int *)_id);
    for (int i=0;i<OUTER_ITERATIONS;++i){
        if (i % 10==0){
            printf("thread: %d at iteration %d\n", id, i);
        }
        for (int j=0;j<INNER_ITERATIONS;++j){
            int random_index = rand() % NUM_OBJECTS;
            int lock_index = get_lock_index(random_index);
            pthread_mutex_lock(&mutexes[lock_index]);
            arr[random_index]++;
            pause_thread(CS_WORK);
            pthread_mutex_unlock(&mutexes[lock_index]);
        }
    }
    return NULL;
}

int main(){
    srand(666);
    //initialize locks
    for (int i=0;i<NUM_MUTEXES;i++){
        pthread_mutex_init(&mutexes[i], NULL);
    }

    //initialize array
    memset((uint8_t *)arr, 0, ARR_SIZE_IN_BYTES); 
    
    int ids[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    for (int i=0;i<NUM_THREADS;++i){
        ids[i]=i;
        pthread_create(&threads[i], NULL, do_work, &ids[i]);
    }

    for (int i=0;i<NUM_THREADS;++i){
        pthread_join(threads[i], NULL);
    }

    if (sum_array()!=NUM_THREADS*INNER_ITERATIONS*OUTER_ITERATIONS){
        fprintf(stderr, "fgl: FAILED\n");   
    }
    else{
        fprintf(stderr, "fgl: SUCCEEDED\n");   
    }
}
