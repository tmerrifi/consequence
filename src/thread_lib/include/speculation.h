#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include <assert.h>

#include "checkpoint.h"
#include "sync_types.h"

#include "conseq_malloc.h"

#include <determ_clock.h>

#ifdef TOKEN_ORDER_ROUND_ROBIN
//basically just infinity...relying on SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 15
#define SPECULATION_MAX_TICKS 500000000
#else

#ifndef SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 100
#endif

#ifndef SPECULATION_MAX_TICKS
#define SPECULATION_MAX_TICKS 30000
#endif

#endif 

#define SPECULATION_ENTRIES_MAX_ALLOCATED (SPECULATION_ENTRIES_MAX+50)

#define SPECULATION_MALLOC_ENTRIES 128

#ifdef USE_CYCLES_TICKS

#define SUCCEEDED_TICK_INC 1000
#define SUCCEEDED_TICK_DEC 10000

#else

#define SUCCEEDED_TICK_INC 500
#define SUCCEEDED_TICK_DEC 2000

#endif

#define SPEC_STATE_FAILED_THREE 1
#define SPEC_STATE_FAILED_TWO 2
#define SPEC_STATE_FAILED_ONE 3
#define SPEC_STATE_SUCCESS_ONE 4
#define SPEC_STATE_SUCCESS_TWO 5
#define SPEC_STATE_SUCCESS_THREE 6

#define SPEC_TRY_AFTER_FAILED 100

#ifdef SPEC_DISABLE_ADAPTATION

//how many tx should we spend "learning"
#define SPEC_LEARNING_PHASE_TX 0
//how often should we kick off a learning phase?
#define SPEC_LEARNING_PHASE_FREQ 1000000000
//how often should we adapt when in a learning phase?
#define SPEC_ADAPTIVE_FREQ_LEARNING 1000000000
//how much adaptation should we do in outside of the learning phase?
#define SPEC_ADAPTIVE_FREQ_NONLEARNING 1000000000

#else

//how many tx should we spend "learning"
#define SPEC_LEARNING_PHASE_TX 500 
//how often should we kick off a learning phase?
#define SPEC_LEARNING_PHASE_FREQ 10000
//how often should we adapt when in a learning phase?
#define SPEC_ADAPTIVE_FREQ_LEARNING 1
//how much adaptation should we do in outside of the learning phase?
#define SPEC_ADAPTIVE_FREQ_NONLEARNING 50

#endif

class speculation{
    
 private:
    class speculation_entry{
    public:
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
    };

    speculation_entry entries[SPECULATION_ENTRIES_MAX_ALLOCATED];
    uint32_t entries_count;
    uint32_t active_speculative_entries;
    uint64_t logical_clock_start;
    checkpoint _checkpoint;
    uint32_t max_entries;
    uint64_t max_ticks;
    uint64_t ticks;
    uint64_t start_ticks;
    uint64_t seq_num;
    uint8_t state;
    bool learning_phase;
    uint32_t learning_phase_count;
    size_t malloc_entries[SPECULATION_MALLOC_ENTRIES];
    size_t free_entries[SPECULATION_MALLOC_ENTRIES];
    uint32_t malloc_entries_count;
    uint32_t free_entries_count;
    
     bool verify_synchronization(){
        for (int i=0;i<entries_count;i++){
            SyncVarEntry * entry = entries[i].entry;
            if (entry->last_committed > logical_clock_start){
                //cout << "failed " << getpid() << " " << entry << endl;
                return false;
            }
        }
        return true;
    }
    
 public:
    speculation(){
        for (int i=0;i<SPECULATION_ENTRIES_MAX_ALLOCATED;++i){
            entries[i].entry=NULL;
        }
        entries_count=0;
        active_speculative_entries=0;
        logical_clock_start=0;
        max_entries=SPECULATION_ENTRIES_MAX;
        max_ticks=SPECULATION_MAX_TICKS;
        state=SPEC_STATE_SUCCESS_ONE;
        seq_num=0;
#ifdef SPEC_DISABLE_ADAPTATION
        learning_phase=false;  
#else
        learning_phase=true;
#endif
        learning_phase_count=0;
        malloc_entries_count=0;
        free_entries_count=0;
    }


    void updateTicks(){
#ifdef SPEC_USE_TICKS
        if (isSpeculating()){
            this->ticks=determ_task_clock_force_read() - start_ticks;   //determ_task_clock_get_last_tx_size();
        }
#else
        
#endif
    }

    void adaptSpeculation(bool succeeded){
        //first, should we adapt at all?
#ifdef SPEC_USE_TICKS
        if ( (!learning_phase && (seq_num % SPEC_ADAPTIVE_FREQ_NONLEARNING) == 0) ||
             (learning_phase && (seq_num % SPEC_ADAPTIVE_FREQ_LEARNING) == 0) ){
            //is the learning phase over????
            if (learning_phase && ++learning_phase_count >= SPEC_LEARNING_PHASE_TX){
                learning_phase=false;
                learning_phase_count=0;
            }
            
            if (succeeded){
                if (this->ticks >= max_ticks){
                    max_ticks+=SUCCEEDED_TICK_INC;
                }
                if (this->state!=SPEC_STATE_SUCCESS_THREE){
                    this->state++;
                }
            }
            else{
                if (this->state!=SPEC_STATE_FAILED_THREE){
                    this->state--;
                }
                max_ticks=(max_ticks<SUCCEEDED_TICK_DEC) ? 0 : max_ticks-SUCCEEDED_TICK_DEC;
            }
        }
#endif
    }

    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock, int * result){
        updateTicks();
#ifdef USE_SPECULATION
        if (state==SPEC_STATE_FAILED_THREE){
            *result=1;
            return false;
        }
        if (active_speculative_entries > 0){
            if (getSyncEntry(entry_ptr)==NULL){
                cout << "nested speculation with uninitialized sync object...not currently supported" << endl;
                exit(-1);
            }
            *result=2;
            //we have to keep going here...otherwise if we stop speculating we'll need to actually acquire the lock
            return true;
        }
        else if (getSyncEntry(entry_ptr)==NULL){
            *result=3;
            return false;
        }
#ifdef SPEC_USE_TICKS
        else if (entries_count >= max_entries){
            //cout << "4: " << entries_count << endl;
            *result=4;
            return false;
        }
        else if (ticks < max_ticks){
            //cout << "5: " << ticks << " " << getpid() << endl;
            *result=5;
            return true;
        }
        else if (ticks >= max_ticks){
            if (isSpeculating()){
                *result=11;
            }
            else{
                *result=12;
            }
            //cout << "11: " << ticks << " " << max_ticks  << " " << getpid() << endl;
            return false; 
        }
#else
        else if (entries_count < max_entries){
            //cout << "6: " << entries_count << endl;
            *result=6;
            return true;
        }
#endif
       
        else{
            *result=7;
            return false;
        }
#else
        //if speculation is disabled
        *result=8;
        return false;
#endif
    }
    
    //called by code that is not adding a new sync var to the current
    //set of entries
     bool shouldSpeculate(uint64_t logical_clock){
#ifdef USE_SPECULATION
        return true;
#else
        //if speculation is disabled
        return false;
#endif
    }
    
     bool speculate(void * entry_ptr, uint64_t logical_clock){
        if (entries_count>=SPECULATION_ENTRIES_MAX_ALLOCATED){
            cout << "Too many speculative entries " << endl;
            exit(-1);
        }
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        if (entry==NULL){
            cout << "SyncVarEntry is null " << endl;
            exit(-1);
        }
        entries[entries_count].entry=entry;
        entries[entries_count].acquisition_logical_time=logical_clock;
        entries_count++;
        active_speculative_entries++;
        if (!_checkpoint.is_speculating){
            logical_clock_start=logical_clock;
            start_ticks = determ_task_clock_read();
            return _checkpoint.checkpoint_begin();
        }
        else{
            return true;
        }

    }

     bool isSpeculating(){
         return _checkpoint.is_speculating;
    }

    
     int validate(){
        if (!isSpeculating()){
            return 0;
        }
        else if (!verify_synchronization() || active_speculative_entries > 0){
            adaptSpeculation(false);
            entries_count=0;
            ticks=0;
            start_ticks=0;
            //need to free everything we malloc'd
            for (int i=0;i<malloc_entries_count;i++){
                conseq_malloc::free((void *)malloc_entries[i]);
            }
            malloc_entries_count=0;
            free_entries_count=0;
            
            //do what we need to do
            _checkpoint.checkpoint_revert();
        }
        adaptSpeculation(true);
        return 1;
    }

     void endSpeculativeEntry(void * entry_ptr){
         assert(active_speculative_entries>0);
         active_speculative_entries--;
     }
     
     void commitSpeculation(uint64_t logical_clock){
         char str[500];
         for (int i=0;i<entries_count;i++){
             SyncVarEntry * entry = entries[i].entry;
             entry->last_committed=logical_clock;
         }
         //free all of the stuff we were asked to free
         for (int i=0;i<free_entries_count;i++){
             conseq_malloc::free((void *)free_entries[i]);
         }
         
         entries_count=0;
         malloc_entries_count=0;
         free_entries_count=0;
         _checkpoint.is_speculating=false;
         ticks=0;
         seq_num++;
         if (!learning_phase && (seq_num % SPEC_LEARNING_PHASE_FREQ) == 0){
             learning_phase=true;
         }
     }

     void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
        seq_num++;
        //maybe we should try speculation again?
        if (this->state==SPEC_STATE_FAILED_THREE &&
            (seq_num % SPEC_TRY_AFTER_FAILED) == 0){
            //by incrementing the state, we'll try speculation once more
            this->state++;
        }
     }

     void addMallocEntry(void * ptr){
         malloc_entries[malloc_entries_count]=(size_t)ptr;
         malloc_entries_count++;
     }
     
     void addFreeEntry(void * ptr){
         free_entries[free_entries_count]=(size_t)ptr;
         free_entries_count++;
     }

     
     int getMallocEntryCount(){
         return malloc_entries_count;
     }

     int getFreeEntryCount(){
         return free_entries_count;
     }
     
     int getEntriesCount(){
         return entries_count;
     }

     uint64_t getCurrentTicks(){
         return ticks;
     }

     uint64_t getMaxTicks(){
         return max_ticks;
     }
};

#endif
