#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include <assert.h>
#include <math.h>
#include "checkpoint.h"
#include "sync_types.h"

#include "conseq_malloc.h"

#include "determ.h"

#include "debug.h"

#include <determ_clock.h>

#ifdef TOKEN_ORDER_ROUND_ROBIN
//basically just infinity...relying on SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 15
#define SPECULATION_MAX_TICKS 500000000
#define SPECULATION_MIN_TICKS 5000
#else
//!TOKEN_ORDER_ROUND_ROBIN
#ifndef SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 100
#endif

//this number will be adjusted during adaptation
#ifndef SPECULATION_MAX_TICKS
#define SPECULATION_MAX_TICKS 30000
#endif

//we never adapt less than this number
#ifndef SPECULATION_MIN_TICKS
#define SPECULATION_MIN_TICKS 10000
#endif


//end of !TOKEN_ORDER_ROUND_ROBIN
#endif 

#define SPECULATION_ENTRIES_MAX_ALLOCATED (SPECULATION_ENTRIES_MAX+50)


#ifdef USE_CYCLES_TICKS

#define SUCCEEDED_TICK_INC 1.1
#define SUCCEEDED_TICK_DEC .8

#else

#define SUCCEEDED_TICK_INC 1.1
#define SUCCEEDED_TICK_DEC .8

#endif

#define SPEC_TRY_AFTER_FAILED 100

#define SPEC_TICKS_TO_HOLD_SIGNAL 10000

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

//percentage of success we are willing to accept
#define SPEC_SYNC_MIN_THRESHOLD .75

#endif

#define EWMA_ALPHA .1F

class speculation{

 public:
    typedef enum {SPEC_ENTRY_LOCK, SPEC_ENTRY_SIGNAL, SPEC_ENTRY_BROADCAST} speculation_entry_type;

    typedef enum { SPEC_TERMINATE_REASON_EXCEEDED_TICKS=1, SPEC_TERMINATE_REASON_EXCEEDED_OBJECT_COUNT=2,
                   SPEC_TERMINATE_REASON_PENDING_SIGNAL=3, SPEC_TERMINATE_REASON_SPEC_DISABLED=4,
                   SPEC_TERMINATE_REASON_NONE=5, SPEC_TERMINATE_REASON_UNINITIALIZED=6,
                   SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_LOCK=7, SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_GLOBAL=8   } spec_terminate_reason_type;
    
 private:
    
    class speculation_entry{
    public:
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
        speculation_entry_type type;
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
    uint64_t failure_count;
    uint64_t tx_count;
    uint64_t signal_delay_ticks;
    double global_success_rate;
        
    bool learning_phase;
    bool buffered_signal;
    uint32_t learning_phase_count;
    spec_terminate_reason_type terminated_spec_reason;

    
    void update_global_success_rate(bool success){
        if (success){
            global_success_rate=(EWMA_ALPHA*100.0) + (global_success_rate*(1.0 - EWMA_ALPHA));
        }
        else{
            global_success_rate=global_success_rate*(1.0 - EWMA_ALPHA);
        }
    }

    
     bool verify_synchronization(){
         int r = rand();
         for (int i=0;i<entries_count;i++){
            SyncVarEntry * entry = entries[i].entry;
            if (entry->last_committed > logical_clock_start){
                //this entry caused us to fail...update its stats
                entry->stats->specFailed();
                failure_count++;
                update_global_success_rate(false);
                cout << " endspec failed entry " << getpid() << " " << entry->id << " " << entry->stats->specPercentageOfSuccess() << " "
                   << " failed " << entry->stats->getFailedCount() << " succeeded " << entry->stats->getSucceededCount() << " " << entry->stats << endl;
                return false;
            }
            else{
                entry->stats->specSucceeded();
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
        buffered_signal=false;
        seq_num=0;
        signal_delay_ticks=0;
        global_success_rate=100.0;
        terminated_spec_reason=SPEC_TERMINATE_REASON_NONE;
#ifdef SPEC_DISABLE_ADAPTATION
        learning_phase=false;  
#else
        learning_phase=true;
#endif
        learning_phase_count=0;
        
        tx_count=10;
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
            /*if (learning_phase && ++learning_phase_count >= SPEC_LEARNING_PHASE_TX){
                learning_phase=false;
                learning_phase_count=0;
                }*/
            if (succeeded && this->ticks >= max_ticks){
                    max_ticks*=SUCCEEDED_TICK_INC;
            }
            else if (!succeeded){
                max_ticks=((max_ticks*SUCCEEDED_TICK_DEC)<SPECULATION_MIN_TICKS) ?
                    SPECULATION_MIN_TICKS : (max_ticks*SUCCEEDED_TICK_DEC);
            }
        }
#endif
    }
        
    //using the multiplication method to do a probabilistic speculation
    bool __shouldAttempt(double percentageOfSuccess){
        const double A=0.432;
        double val = floor(100 * ( ((double)seq_num*A) - floor((double)seq_num*A)));
        //cout << "\% of success " << percentageOfSuccess << " val " << val << " " << (val<percentageOfSuccess) << endl;
        return val < percentageOfSuccess;
    }

#ifdef USE_SPECULATION
    
    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock, int * result){
        bool return_val;
        updateTicks();
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);

        if (active_speculative_entries > 0){
            if (getSyncEntry(entry_ptr)==NULL){
                cout << "nested speculation with uninitialized sync object...not currently supported" << endl;
                exit(-1);
            }
            //we have to keep going here...since we're still "holding" a lock. 
            return_val=true;
        }
        else if (!isSpeculating() && !__shouldAttempt( global_success_rate )){
            terminated_spec_reason = SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_GLOBAL;
            return_val=false;
        }
        else if (getSyncEntry(entry_ptr)==NULL){
            terminated_spec_reason = SPEC_TERMINATE_REASON_UNINITIALIZED;
            return_val=false;
        }
        //if we're about to speculate on a lock that is likely to cause a conflict, lets not to it
        else if (entry->stats->specPercentageOfSuccess() < SPEC_SYNC_MIN_THRESHOLD ){
            terminated_spec_reason = SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_LOCK;
            return_val=false;
        }
#ifdef SPEC_USE_TICKS
        else if (entries_count >= max_entries){
            terminated_spec_reason = SPEC_TERMINATE_REASON_EXCEEDED_OBJECT_COUNT;
            return_val=false;
        }
        else if (ticks >= max_ticks){
            terminated_spec_reason = SPEC_TERMINATE_REASON_EXCEEDED_TICKS;
            return_val=false; 
        }
        else if (buffered_signal==true && (ticks - signal_delay_ticks) > SPEC_TICKS_TO_HOLD_SIGNAL) {
            terminated_spec_reason = SPEC_TERMINATE_REASON_PENDING_SIGNAL;
            return_val=false;            
        }
#else
        //lets kill it if we are buffering a signal. Don't want to delay other threads due to speculation
        else if (buffered_signal==true){
            terminated_spec_reason = SPEC_TERMINATE_REASON_PENDING_SIGNAL;
            return_val=false;
        }
#endif
        else{
            return_val=true;
        }


        if (return_val==false){
            cout << "endspec " << getpid() << " " << terminated_spec_reason << " " << max_ticks << " " << ticks << " " << entries_count << " "
                 << entry->stats->specPercentageOfSuccess() << " " << entry->id << endl;
        }
        
        if (terminated_spec_reason==SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_GLOBAL){
            cout << "globalfail " << getpid() << " " << getPercentageOfSuccess() << " " << global_success_rate << endl;
            }
        
        return return_val;
    }
    
    //called by code that is not adding a new sync var to the current
    //set of entries
    bool shouldSpeculate(uint64_t logical_clock){
        return true;
    }
#else

    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock, int * result){
        return false;
    }

    bool shouldSpeculate(uint64_t logical_clock){
        return false;
    }

    
#endif
     
     bool speculate(void * entry_ptr, uint64_t logical_clock, speculation_entry_type type){
        if (entries_count>=SPECULATION_ENTRIES_MAX_ALLOCATED){
            cout << "Too many speculative entries " << endl;
            exit(-1);
        }
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        if (entry==NULL){
            cout << "SyncVarEntry is null " << endl;
            exit(-1);
        }
        if (type == SPEC_ENTRY_SIGNAL || type == SPEC_ENTRY_BROADCAST){
            buffered_signal=true;
            signal_delay_ticks=ticks;
        }
        else if (type == SPEC_ENTRY_LOCK) {
            active_speculative_entries++;
        }
        
        entries[entries_count].entry=entry;
        entries[entries_count].type=type;
        entries[entries_count].acquisition_logical_time=logical_clock;
        entries_count++;
        if (!_checkpoint.is_speculating){
            logical_clock_start=logical_clock;
            start_ticks = determ_task_clock_read();
            terminated_spec_reason = SPEC_TERMINATE_REASON_NONE;
            tx_count++;
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
            cout << " endspec failed " << getpid() << endl;
            adaptSpeculation(false);
            entries_count=0;
            ticks=0;
            start_ticks=0;
            active_speculative_entries=0;
            buffered_signal=false;
            signal_delay_ticks=0;
            _checkpoint.checkpoint_revert();
        }
        else{
            adaptSpeculation(true);
            update_global_success_rate(true);
            return 1;
        }
    }

     void endSpeculativeEntry(void * entry_ptr){
         assert(active_speculative_entries>0);
         active_speculative_entries--;
     }
     
     void commitSpeculation(uint64_t logical_clock){
         char str[500];

         for (int i=0;i<entries_count;i++){
             SyncVarEntry * entry = entries[i].entry;
             if (entries[i].type == SPEC_ENTRY_LOCK){
                 entry->last_committed=logical_clock;
             }
             else if (entries[i].type == SPEC_ENTRY_SIGNAL) {
                 //send the signal we buffered
                 determ::getInstance().cond_signal_inner((CondEntry *)entry);
             }
             else if (entries[i].type == SPEC_ENTRY_BROADCAST) {
                 //send the signal we buffered
                 determ::getInstance().cond_broadcast_inner((CondEntry *)entry);
             }
         }
         cout << "endspec commit " << entries_count << " " << max_ticks << " " << getpid() << endl;
         entries_count=0;
         buffered_signal=false;
         signal_delay_ticks=0;
         _checkpoint.is_speculating=false;
         ticks=0;
         seq_num++;
         
         /*if (!learning_phase && (seq_num % SPEC_LEARNING_PHASE_FREQ) == 0){
             learning_phase=true;
             }*/
     }

     void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
        seq_num++;
     }

     int getActiveEntriesCount(){
         return active_speculative_entries;
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

     spec_terminate_reason_type getTerminateReasonType(){
         return terminated_spec_reason;
     }

     double getPercentageOfSuccess(){
         return (((double)(tx_count - failure_count))/(double)tx_count)*100.0;
     }

     uint64_t getTxCount(){
         return tx_count;
     }
};

#endif
