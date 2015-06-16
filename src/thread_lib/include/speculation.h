#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include "checkpoint.h"
#include "sync_types.h"

#define SPECULATION_ENTRIES_MAX 100

class speculation{
    
 private:
    class speculation_entry{
    public:
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
    };

    speculation_entry entries[SPECULATION_ENTRIES_MAX];
    uint32_t entries_count;
    uint64_t logical_clock_start;
    checkpoint _checkpoint;

    inline bool verify_synchronization(){
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
        cout << "speculation constructor " << endl;
        for (int i=0;i<SPECULATION_ENTRIES_MAX;++i){
            entries[i].entry=NULL;
        }
        entries_count=0;
        logical_clock_start=0;
    }
    
    inline bool shouldSpeculate(void * entry_ptr){
        //in the event that the sync variable has not yet been initialized, just
        //forget the speculation for now
        if (entries_count>=SPECULATION_ENTRIES_MAX ||
            getSyncEntry(entry_ptr)==NULL){
            return false;
        }
        else{
            //TODO: need to make a decent decision about when to do this
            return true;
        }
    }

    inline bool speculate(void * entry_ptr, uint64_t logical_clock){
        if (entries_count>=SPECULATION_ENTRIES_MAX){
            cout << "Too many speculative entries " << endl;
            exit(-1);
        }
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        if (entry==NULL){
            cout << "SyncVarEntry is null " << endl;
            exit(-1);
        }
        entries_count++;
        entries[entries_count].entry=entry;
        entries[entries_count].acquisition_logical_time=logical_clock;
        if (!_checkpoint.is_speculating){
            logical_clock_start=logical_clock;
            return _checkpoint.checkpoint_begin();
        }
        else{
            return true;
        }

    }

    inline bool isSpeculating(){
        return _checkpoint.is_speculating;
    }

    
    inline void validate(){
        if (isSpeculating() && !verify_synchronization()){
            entries_count=0;
            //do what we need to do
            _checkpoint.checkpoint_revert();
        }
    }

    inline void finish(uint64_t logical_clock){
        for (int i=0;i<entries_count;i++){
            SyncVarEntry * entry = entries[i].entry;
            entry->last_committed=logical_clock;
        }
        entries_count=0;
        _checkpoint.is_speculating=false;
    }

    inline void updateLastCommittedTime(void * entry_ptr, uint64_t logical_clock){
        SyncVarEntry * entry=(SyncVarEntry *)getSyncEntry(entry_ptr);
        entry->last_committed=logical_clock;
    }
};

#endif
