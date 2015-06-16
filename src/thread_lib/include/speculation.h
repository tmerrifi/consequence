#ifndef CONSEQ_SPECULATION_H
#define CONSEQ_SPECULATION_H

#include "checkpoint.h"

#define SPECULATION_ENTRIES_MAX 100;

class speculation{
    
 private:
    class speculation_entry{
        SyncVarEntry * entry;
        uint64_t acquisition_logical_time;
    };

    speculation_entry entries[SPECULATION_ENTRIES_MAX];
    uint32_t entries_count;
    uint64_t logical_clock_start;
    checkpoint checkpoint;

    inline bool verify_synchronization(){
        
    }
    
 public:
    speculation(){
        cout << "speculation constructor " << endl;
        for (int i=0;i<SPECULATION_ENTRIES_MAX;++i){
            entries[i].entry=NULL;
        }
        entries_count=0;
        logical_clock_start=0;
        is_speculating=false;
    }
    
    inline bool shouldSpeculate(SyncVarEntry * entry){
        if (entries_count>=SPECULATION_ENTRIES_MAX){
            return false;
        }
        else{
            //TODO: need to make a decent decision about when to do this
            return true;
        }
    }

    inline bool speculate(SyncVarEntry * entry, uint64_t logical_clock){
        if (entries_count>=SPECULATION_ENTRIES_MAX){
            cout << "Too many speculative entries " << endl;
            exit(-1);
        }
        entries_count++;
        entries[entries_count].entry=entry;
        entries[entries_count].acquisition_logical_time=logical_clock;
        if (!checkpoint.is_speculating){
            logical_clock_start=logical_clock;
            return checkpoint.checkpoint_begin();
        }
        else{
            return true;
        }

    }

    inline bool isSpeculating(){
        return checkpoint.is_speculating;
    }

    
    inline void validate(){
        if (isSpeculating() && !verify_synchronization()){
            entries_count=0;
            //do what we need to do
            checkpoint.checkpoint_revert();
        }
    }
    
};

#endif
