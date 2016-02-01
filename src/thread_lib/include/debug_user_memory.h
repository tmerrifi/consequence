
#ifndef DEBUG_USER_MEMORY
#define DEBUG_USER_MEMORY

#define MAX_DEBUG_ENTRIES 300
#define MAX_MEM_ENTRIES 20
#define MESSAGE_SIZE 50


#include <determ_clock.h>


class debug_user_memory{

    class debug_user_memory_entry{
    public:
        uint32_t memory[MAX_MEM_ENTRIES];
        int length;
        int seq_num;
        uint64_t clock;
        char message[MESSAGE_SIZE];
    };
    
 public:
    debug_user_memory(int _tid){
        entries_count=0;
        tid=_tid;
    }

    void add(uint32_t * mem, char * message, int length, int seq_num){
#ifdef DEBUG_USER_MEMORY_ON
        if (entries_count < MAX_DEBUG_ENTRIES){
            memcpy(entries[entries_count].memory, mem, ((length < MAX_MEM_ENTRIES) ? length : MAX_MEM_ENTRIES)*sizeof(uint32_t) );
            entries[entries_count].length=(length < MAX_MEM_ENTRIES) ? length : MAX_MEM_ENTRIES;
            strncpy(entries[entries_count].message, message, (strlen(message) < MESSAGE_SIZE) ? strlen(message) : MESSAGE_SIZE);
            entries[entries_count].seq_num=seq_num;
            entries[entries_count].clock=determ_task_clock_read();
            entries_count++;
        }
#endif
    }

    void print(){
#ifdef DEBUG_USER_MEMORY_ON
        for (int i=0;i<entries_count;i++){
            cout << "ENTRY tid: " << tid << " seq: " << entries[i].seq_num << " clock: " << entries[i].clock << " " << entries[i].message << "  mem: ";
            for (int j=0;j<entries[i].length;j++){
                cout << entries[i].memory[j] << ",  ";
            }
            cout << endl;
        }
#endif
    }
    
 private:
    debug_user_memory_entry entries[MAX_DEBUG_ENTRIES];
    int entries_count;
    int tid;
};

#endif
