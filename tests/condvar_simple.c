
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>


#define PRODUCER_THREADS 2
#define CONSUMER_THREADS 2

#define ITERATIONS 50

pthread_mutex_t mutex;
pthread_cond_t cond;

int avail=0;
int consumed=0;

int __attribute__((optimize(0))) pause_thread(int pause_count){
    int total=0;
    for(int i=0;i<pause_count;++i){
        total++;
    }
    return total;
}

void * produce(void * arg){
    int id = *((int *)arg);
    for(int i=0;i<ITERATIONS;++i){
        pause_thread(1000000);
        pthread_mutex_lock(&mutex);
        avail++;
        printf("PRODUCED: thread %d pid: %d val %d\n", id, getpid(), avail);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond);
    }
    return NULL;
}

void * consume(void * arg){
    int id = *((int *)arg);
    for(int i=0;i<ITERATIONS;++i){
        pause_thread(10000);
        pthread_mutex_lock(&mutex);
        while(avail<=0){
            pthread_cond_wait(&cond, &mutex);
        }
        avail--;
        ++consumed;
        printf("CONSUMED: thread %d pid %d val %d consumed %d\n", id, getpid(), avail, consumed);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}


int main(){
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    //create producers
    pthread_t producers[PRODUCER_THREADS];
    int producer_ids[PRODUCER_THREADS];
    for (int i=0;i<PRODUCER_THREADS;++i){
        producer_ids[i]=i;
        pthread_create(&producers[i], NULL, produce, &producer_ids[i]);
    }

    //create consumers
    pthread_t consumers[CONSUMER_THREADS];
    int consumer_ids[CONSUMER_THREADS];
    for (int i=0;i<CONSUMER_THREADS;++i){
        consumer_ids[i]=i+PRODUCER_THREADS;
        pthread_create(&consumers[i], NULL, consume, &consumer_ids[i]);
    }


    //join producers
    for (int i=0;i<PRODUCER_THREADS;++i){
        pthread_join(producers[i],NULL);
    }

    //sleep(5);
    
    //kill off consumers
    for (int i=0;i<CONSUMER_THREADS;++i){
        pthread_join(consumers[i],0);
    }

    if (consumed==PRODUCER_THREADS*ITERATIONS && avail==0){
        fprintf(stderr, "condvar_simple: SUCCEEDED\n");
    }
    else{
        fprintf(stderr, "condvar_simple: FAILED\n");     
    }
}
