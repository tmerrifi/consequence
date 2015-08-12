#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include "checkpoint.h"
#include "sync_types.h"

#define SPECULATION_ENTRIES_MAX_END 15

#define SPECULATION_ENTRIES_MAX_BEGIN 10

class speculation{
    
 private:
    class speculation_entry{
    public:
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
    };

    speculation_entry entries[SPECULATION_ENTRIES_MAX_END];
    uint32_t entries_count;
    uint32_t active_speculative_entries;
    uint64_t logical_clock_start;
    checkpoint _checkpoint;

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
        for (int i=0;i<SPECULATION_ENTRIES_MAX_END;++i){
            entries[i].entry=NULL;
        }
        entries_count=0;
        active_speculative_entries=0;
        logical_clock_start=0;
        cout << "constructor " << &_checkpoint.is_speculating << endl;
    }
    
    bool shouldSpeculate(void * entry_ptr, uint64_t logical_clock){

        //in the event that our speculation is nested, we can't terminate if there are active, speculative
        //locks that haven't yet been released
        if (active_speculative_entries>0 && entries_count < SPECULATION_ENTRIES_MAX_END){
            return true;
        }
        else if (getSyncEntry(entry_ptr)==NULL ||
                 (active_speculative_entries==0 && entries_count >= SPECULATION_ENTRIES_MAX_BEGIN) ||
                 entries_count >= SPECULATION_ENTRIES_MAX_END)
            {            
                
                return false;
            }
        else{
            //TODO: need to make a decent decision about when to do this
            return true;
        }
    }


    //called by code that is not adding a new sync var to the current
    //set of entries
     bool shouldSpeculate(uint64_t logical_clock){
        return true;
    }
    
     bool speculate(void * entry_ptr, uint64_t logical_clock){
        if (entries_count>=SPECULATION_ENTRIES_MAX_END){
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
            cout << "beginning checkpoint " << getpid() << endl;
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
            //do what we need to do
            _checkpoint.checkpoint_revert();
        }
        return 1;
    }

     void endSpeculativeEntry(void * entry_ptr){
         active_speculative_entries--;
     }
     
     void commitSpeculation(uint64_t logical_clock){
         cout << "commitSpeculation " << endl;
         for (int i=0;i<entries_count;i++){
             SyncVarEntry * entry = entries[i].entry;
             entry->last_committed=logical_clock;
         }
         entries_count=0;
         _checkpoint.is_speculating=false;
     }

     void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
    }
};

#endif
