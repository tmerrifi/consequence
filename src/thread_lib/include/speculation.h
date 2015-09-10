#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include <assert.h>

#include "checkpoint.h"
#include "sync_types.h"

#include <determ_clock.h>

#define SPEC_USE_TICKS 1

#define SPECULATION_ENTRIES_MAX 15

#define SPECULATION_MAX_TICKS 100000

#define SPECULATION_ENTRIES_MAX_ALLOCATED (SPECULATION_ENTRIES_MAX+100)

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
    uint64_t ticks;
    
     bool verify_synchronization(){
        for (int i=0;i<entries_count;i++){
            SyncVarEntry * entry = entries[i].entry;
            if (entry->last_committed > logical_clock_start){
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
    }


    void updateTicks(){
#ifdef SPEC_USE_TICKS
        this->ticks+=determ_task_clock_get_last_tx_size();
#else
        
#endif
    }

    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock){
        
        if (active_speculative_entries > 0){
            if (getSyncEntry(entry_ptr)==NULL){
                cout << "nested speculation with uninitialized sync object...not currently supported" << endl;
                exit(-1);
            }
            //we have to keep going here...otherwise if we stop speculating we'll need to actually acquire the lock
            return true;
        }
        else if (getSyncEntry(entry_ptr)==NULL){
            return false;
        }
#ifdef SPEC_USE_TICKS
        else if (entries_count > SPECULATION_ENTRIES_MAX){
            return false;
        }
        else if (ticks < SPECULATION_MAX_TICKS){
            return true;
        }
#else
        else if (entries_count < SPECULATION_ENTRIES_MAX){
            return true;
        }
#endif
        else{
            return false;
        }
    }
    
    //called by code that is not adding a new sync var to the current
    //set of entries
     bool shouldSpeculate(uint64_t logical_clock){
        return true;
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
        else if (!verify_synchronization()){
            entries_count=0;
            ticks=0;
            //do what we need to do
            _checkpoint.checkpoint_revert();
        }
        return 1;
    }

     void endSpeculativeEntry(void * entry_ptr){
         assert(active_speculative_entries>0);
         active_speculative_entries--;
     }
     
     void commitSpeculation(uint64_t logical_clock){
         for (int i=0;i<entries_count;i++){
             SyncVarEntry * entry = entries[i].entry;
             entry->last_committed=logical_clock;
         }
         entries_count=0;
         _checkpoint.is_speculating=false;
         ticks=0;
     }

     void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
     }

     int getEntriesCount(){
         return entries_count;
     }
};

#endif
