#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include <assert.h>
#include <math.h>
#include "checkpoint.h"
#include "sync_types.h"
#include "conseq_malloc.h"
#include "determ.h"
#include "debug.h"
#include "simple_stack.h"
#include <determ_clock.h>
#include <perfcounterlib.h>

#ifdef USE_FTRACE_DEBUGGING
#include <ftrace.h>
#define FTRACE_FREQUENCY 100 //do an ftrace capture every N tx's
#endif


#ifdef TOKEN_ORDER_ROUND_ROBIN
//basically just infinity...relying on SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 50
#define SPECULATION_START_TICKS 500000000
#define SPECULATION_MAX_TICKS 500000000
#define SPECULATION_MIN_TICKS 5000
#else
//!TOKEN_ORDER_ROUND_ROBIN
#ifndef SPECULATION_ENTRIES_MAX
#define SPECULATION_ENTRIES_MAX 1024
#endif

#ifndef SPECULATION_START_TICKS
#define SPECULATION_START_TICKS 30000
#endif

//this number will be adjusted during adaptation
#ifndef SPECULATION_MAX_TICKS
#define SPECULATION_MAX_TICKS 150000
#endif

//we never adapt less than this number
#ifndef SPECULATION_MIN_TICKS
#define SPECULATION_MIN_TICKS 1000
#endif

//how many locks can we do during fast path?
#ifndef SPECULATION_FAST_PATH_DEPTH
#define SPECULATION_FAST_PATH_DEPTH 20
#endif

//end of !TOKEN_ORDER_ROUND_ROBIN
#endif 

#define SPECULATION_START_MAX_SYNC_OBJS 512

#define SPECULATION_ENTRIES_MAX_ALLOCATED (SPECULATION_ENTRIES_MAX*2)

#define SUCCEEDED_TICK_INC 5000
#define SUCCEEDED_TICK_DEC_MULT .5
#define SPEC_TRY_AFTER_FAILED 50
#define SPEC_TICKS_TO_HOLD_SIGNAL 10000

#define SPECULATION_MAX_SYNC_OBJECTS 50

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
#define SPEC_SYNC_MIN_THRESHOLD .85

#endif

#define EWMA_ALPHA .1F

//only try if we believe this is going to work this percentage of the time
#define SPEC_ATTEMPT_THRESHOLD .70

//if we don't meet the threshold, try again 20 times
#define SPEC_ATTEMPT_AGAIN 100

#define SPEC_FAILURE_PENALTY_NORMAL 1
#define SPEC_FAILURE_PENALTY_HIGH 10


class speculation{

 public:
    typedef enum {SPEC_ENTRY_LOCK, SPEC_ENTRY_SIGNAL, SPEC_ENTRY_BROADCAST} speculation_entry_type;

    typedef enum { SPEC_TERMINATE_REASON_EXCEEDED_TICKS=1, SPEC_TERMINATE_REASON_EXCEEDED_OBJECT_COUNT=2,
                   SPEC_TERMINATE_REASON_PENDING_SIGNAL=3, SPEC_TERMINATE_REASON_SPEC_DISABLED=4,
                   SPEC_TERMINATE_REASON_NONE=5, SPEC_TERMINATE_REASON_UNINITIALIZED=6,
                   SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_LOCK=7, 
                   SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_GLOBAL=8,
                   SPEC_TERMINATE_REASON_INEVITABLE=9 } spec_terminate_reason_type;
    
 private:
    
    class speculation_entry{
    public:
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
        speculation_entry_type type;
    };

    speculation_entry entries[SPECULATION_ENTRIES_MAX_ALLOCATED];
    
    
    uint32_t entries_count;
    uint32_t locks_count;
    uint32_t active_speculative_entries;
    uint64_t logical_clock_start;
    uint64_t last_seen_logical_clock; //the logical clock of the last non-fast path trip through shouldSpeculate
    checkpoint _checkpoint;
    uint32_t max_entries;
    uint32_t max_sync_objs;
    uint64_t max_ticks;
    uint64_t ticks;
    uint64_t start_ticks;
    uint64_t seq_num;
    uint64_t failure_count;
    uint64_t successful_commits;
    uint64_t tx_count;
    uint64_t signal_delay_ticks;
    double global_success_rate;
    int tid;
    bool learning_phase;
    bool buffered_signal;
    bool inevitable;
    uint32_t learning_phase_count;
    SyncVarEntry * entry_ended_spec;
    struct timespec tx_start_time;
    struct timespec tx_end_time;
#ifdef USE_FTRACE_DEBUGGING
    struct ftracer * tracer;
#endif

    //****STATS***
    unsigned long revert_cs;
    unsigned long spec_cs;

    /**** A stack to use for keeping track of nested critical sections*****/
    struct simple_stack * nested_stack;
    
    spec_terminate_reason_type terminated_spec_reason;

    int perf_counter;

    void print_active_sync_objects(){
        for (int i=0;i<entries_count;i++){
            cout << " , " << entries[i].entry->id << " " << entries[i].type;
        }
    }
    
    void update_global_success_rate(bool success){
        if (success){
            global_success_rate=(EWMA_ALPHA*100.0) + (global_success_rate*(1.0 - EWMA_ALPHA));
        }
        else{
            global_success_rate=global_success_rate*(1.0 - EWMA_ALPHA);
        }
    }


    inline bool ALWAYS_INLINE __last_committed_is_larger(SyncVarEntry * entry, uint64_t clock, int tid){
        return (entry->last_committed > clock ||
                entry->last_committed==clock && entry->committed_by != tid);
    }
    
    inline bool ALWAYS_INLINE __verify_sync_entry(SyncVarEntry *entry, uint64_t clock, speculation_entry_type entry_type, int tid) {
        //its possible that a lock was acquired prior to the start of our speculation, and yet still held. In that case, we will see that the "last_committed" field
        //is less than our start time. Therefore, we need to verify that the lock is not currently held "for real."
        bool lockHeld = (entry_type == SPEC_ENTRY_LOCK) ? ((LockEntry *)entry)->is_acquired : false;
        return !lockHeld && !__last_committed_is_larger(entry, clock, tid);
    }

    bool verify_synchronization(){
        int r = rand();
        for (int i=0;i<entries_count;i++){
            SyncVarEntry * entry = entries[i].entry;
            if (__verify_sync_entry(entry, logical_clock_start, entries[i].type, tid)) {
                update_global_success_rate(true);
            }
            else{
                //this entry caused us to fail...update its stats
                update_global_success_rate(false);
                specStatsFailed(entry, tid, SPEC_FAILURE_PENALTY_NORMAL);
                return false;
            }
        }
        return true;
    }
    
 public:
     
     speculation(int _tid){
        for (int i=0;i<SPECULATION_ENTRIES_MAX_ALLOCATED;++i){
            entries[i].entry=NULL;
        }
        tid=_tid;
        entries_count=0;
        locks_count=0;
        active_speculative_entries=0;
        logical_clock_start=0;
        max_entries=SPECULATION_ENTRIES_MAX;
        max_ticks=SPECULATION_START_TICKS;
	max_sync_objs = SPECULATION_START_MAX_SYNC_OBJS;
        buffered_signal=false;
        inevitable=false;
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
#ifdef USE_FTRACE_DEBUGGING
        tracer = ftrace_init();
#endif
        
#ifdef USE_DEBUG_COUNTER
        perf_counter=perfcounterlib_open(PERF_TYPE_RAW, (0x003CULL) | (0x0000ULL));
#endif //END USE_DEBUG_COUNTER
        revert_cs=0;
        spec_cs=0;
        successful_commits=0;
        failure_count=0;
        nested_stack=simple_stack_init(SPECULATION_ENTRIES_MAX_ALLOCATED);
    }


    void updateTicks(){
#ifdef SPEC_USE_TICKS
        if (isSpeculating()){
            this->ticks=determ_task_clock_read() - start_ticks;
        }
#else
        
#endif
    }

    void adaptSpeculation(bool succeeded){
#ifdef SPEC_USE_TICKS
        if (succeeded){
            max_ticks = xmin(max_ticks + SUCCEEDED_TICK_INC, SPECULATION_MAX_TICKS);
            max_sync_objs = xmin(max_sync_objs + 1, SPECULATION_ENTRIES_MAX);
        }
        else{
            max_ticks = xmax(max_ticks/2, SPECULATION_MIN_TICKS);
            max_sync_objs = xmax(max_sync_objs/2, 1);
        }
#endif
    }
    
    //using the multiplication method to do a probabilistic speculation
    bool __shouldAttempt(double percentageOfSuccess){
        return (percentageOfSuccess >= SPEC_ATTEMPT_THRESHOLD ||
                seq_num % SPEC_ATTEMPT_AGAIN == 0);       
    }

#ifdef USE_SPECULATION


    static unsigned long long __rdtsc(void)
    {
        unsigned long low, high;
        asm volatile("rdtsc" : "=a" (low), "=d" (high));
        return ((low) | (high) << 32);
    }


    bool inline shouldSpeculateFastPathLock(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry *entry = (SyncVarEntry *)getSyncEntry(entry_ptr);
	if (entries_count < SPECULATION_ENTRIES_MAX_ALLOCATED && entries_count < max_sync_objs &&
	    entry != NULL && (active_speculative_entries > 0 || seq_num % SPECULATION_FAST_PATH_DEPTH != 0) &&
	    __shouldAttempt(specStatsSucceededLastTime(entry, tid))) {
            entries[entries_count].entry=entry;
            entries[entries_count].type=SPEC_ENTRY_LOCK;
            entries[entries_count].acquisition_logical_time=logical_clock;
            entries_count++;
            active_speculative_entries++;
            seq_num++;
            return true;
        }
        else{
            return false;
        }
    }
    
    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock, int * result, speculation_entry_type entry_type){
        bool return_val;
        unsigned long long startcycles = __rdtsc();
        updateTicks();
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        
        if (active_speculative_entries > 0){
            if (getSyncEntry(entry_ptr)==NULL){
                cout << "nested speculation with uninitialized sync object...not currently supported" << endl;
                exit(-1);
            }
            //we have to keep going here...since we're still "holding" a lock.
            last_seen_logical_clock=logical_clock;
            return_val=true;
        }
        else if (entry==NULL){
            terminated_spec_reason = SPEC_TERMINATE_REASON_UNINITIALIZED;
            return_val=false;
        }
        else if (inevitable) {
           terminated_spec_reason = SPEC_TERMINATE_REASON_INEVITABLE;
           return_val=false;
        }
        //if we're about to speculate on a lock that is likely to cause a conflict, lets not do it
        else if (!__shouldAttempt( specStatsSuccessRate(entry,tid))){
            terminated_spec_reason = SPEC_TERMINATE_REASON_SPEC_MAY_FAIL_LOCK;
            entry_ended_spec=entry;
            return_val=false;
        }
        else if (entries_count >= SPECULATION_ENTRIES_MAX || entries_count >= max_sync_objs){
            terminated_spec_reason = SPEC_TERMINATE_REASON_EXCEEDED_OBJECT_COUNT;
            entry_ended_spec=entry;
            return_val=false;
        }
#ifdef SPEC_USE_TICKS
        else if (entries_count >= max_entries){
            terminated_spec_reason = SPEC_TERMINATE_REASON_EXCEEDED_OBJECT_COUNT;
            return_val=false;
        }
        //ignore every SPEC_ATTEMPT_AGAIN
        else if (ticks >= max_ticks && !(seq_num % SPEC_ATTEMPT_AGAIN == 0) ){
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
        else if (isSpeculating() && !__verify_sync_entry(entry, logical_clock_start, entry_type, tid)) {
            return_val=false;
        }
        else {
            last_seen_logical_clock=logical_clock;
            return_val=true;
        }

#ifdef USE_FTRACE_DEBUGGING
        if (return_val==false && tx_count%FTRACE_FREQUENCY==0){
            char end_message[100];
            determ_task_clock_force_read();
            updateTicks();
            clock_gettime(CLOCK_REALTIME, &tx_end_time);
            unsigned long diff = time_util_time_diff(&tx_start_time, &tx_end_time);
            sprintf(end_message, "ending-tx...%lu %lu %d %d", diff, ticks, seq_num, getpid());
            ftrace_write_to_trace(tracer, end_message);
            ftrace_off(tracer);
        }
#endif //END FTRACE
        return return_val;
    }
    
    //called by code that is not adding a new sync var to the current
    //set of entries
    bool shouldSpeculate(uint64_t logical_clock){
        return true;
    }
#else

    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock, int * result, speculation_entry_type type){
        return false;
    }

    bool shouldSpeculate(uint64_t logical_clock){
        return false;
    }

    bool inline shouldSpeculateFastPathLock(void * entry_ptr, uint64_t logical_clock){
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
            simple_stack_push(nested_stack, entry);
            locks_count++;
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
#ifdef USE_FTRACE_DEBUGGING
            if (tx_count%FTRACE_FREQUENCY==0){
                char message[100];
                sprintf(message,"starting tx %d\n", getpid());
                ftrace_on(tracer);
                ftrace_write_to_trace(tracer, message);
            }
#endif //ENDING FTRACE
            
#ifdef USE_DEBUG_COUNTER
            perfcounterlib_start(perf_counter);
#endif //ENDING DEBUG_COUNTER            
            clock_gettime(CLOCK_REALTIME, &tx_start_time);
            return _checkpoint.checkpoint_begin();
        }
        else{
            return true;
        }

    }

    bool isSpeculating(){
         return _checkpoint.is_speculating;
    }

     
     int validate(bool forcedTerminate){
         bool result;
         
        if (!isSpeculating()){
            return 0;
        }
        else if (!(result=verify_synchronization()) || active_speculative_entries > 0){
            if (forcedTerminate && active_speculative_entries>0){
                //if this happens we need to make sure we don't speculate on this lock when we retry. We also take this one step further
                //and heavily penalize the lock, as it appears to be protecting a CS that is going to do something we don't like (system call?).
                //So we find all the active (or nested) locks and penalize them
                SyncVarEntry * active_entry;
                while((active_entry=(SyncVarEntry *)simple_stack_pop(nested_stack))!=NULL){
                    specStatsFailed(active_entry, tid, SPEC_FAILURE_PENALTY_HIGH);
                }
            }
            adaptSpeculation(false);
            revert_cs+=entries_count;
            failure_count++;
            entries_count=0;
            ticks=0;
            start_ticks=0;
            inevitable=false;
            locks_count=0;
            active_speculative_entries=0;
            simple_stack_clear(nested_stack);
            buffered_signal=false;
            signal_delay_ticks=0;
            //cout << "Reverting..." << tid << " " << getpid() << endl;
            _checkpoint.checkpoint_revert();
        }
        else{
            adaptSpeculation(true);
            //cout << "Success..." << tid << " " << getpid() << endl;
            return 1;
        }
    }

     bool makeInevitable(){
        if (!inevitable) {
           inevitable = verify_synchronization();
        }
        return inevitable;
     }

     void endSpeculativeEntry(void * entry_ptr){
         assert(active_speculative_entries>0);
         active_speculative_entries--;
         simple_stack_pop(nested_stack);
     }
     
     void commitSpeculation(uint64_t logical_clock){
         char str[500];

         for (int i=0;i<entries_count;i++){
             SyncVarEntry * entry = entries[i].entry;
             bool firstCommitOfEntry = (entry->last_committed!=logical_clock);
             
             if (entries[i].type == SPEC_ENTRY_SIGNAL) {
                 //send the signal we buffered
                 determ::getInstance().cond_signal_inner((CondEntry *)entry);
             }
             else if (entries[i].type == SPEC_ENTRY_BROADCAST) {
                 //send the signal we buffered
                 determ::getInstance().cond_broadcast_inner((CondEntry *)entry);
             }

             if (firstCommitOfEntry){
                 //update the stats
                 specStatsSuccess(entry, tid);
                 //entry->getStats(tid)->specSucceeded();
             }
             
             entry->last_committed=logical_clock;
             entry->committed_by=tid;
         }
         spec_cs+=entries_count;
         entries_count=0;
         locks_count=0;
         buffered_signal=false;
         inevitable=false;
         signal_delay_ticks=0;
         _checkpoint.is_speculating=false;
         ticks=0;
         successful_commits++;
         seq_num++;
         simple_stack_clear(nested_stack);
     }

     void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
        entry->committed_by=tid;
        seq_num++;
        //cout << "updatelastcomm tid: " << tid << " " << entry << " " << logical_clock << endl;

     }

     bool isInevitable(){
        return inevitable;
     }

     int getActiveEntriesCount(){
         return active_speculative_entries;
     }
     
     int getEntriesCount(){
         return entries_count;
     }

     int getLockCount(){
         return locks_count;
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

     double meanRevertCS(){
         return (double)revert_cs/(double)failure_count;
     }

     double meanSpecCS(){
         return (double)spec_cs/(double)successful_commits;
     }

     unsigned long getReverts(){
         return failure_count;
     }

     unsigned long getCommits(){
         return successful_commits;
     }

     unsigned long getLogicalClockStart(){
         return logical_clock_start;
     }

};

#endif
