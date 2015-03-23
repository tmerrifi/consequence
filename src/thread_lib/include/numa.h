#ifndef NUMA_DET_H

#define NUMA_CONSECUTIVE_ACQUIRES 10

#include <syscall.h>
#include <sched.h>

//here we get the current NUMA node and set the CPU affinity
static inline uint32_t init_numa(){
    unsigned numa_node, cpu;
    syscall(__NR_getcpu, &cpu, &numa_node, NULL);
    //getcpu(&cpu, &numa_node, NULL);
    cout << "thread is on cpu " << cpu << " and numa node " << numa_node << endl;
    //now set affinity for our current 
}


#endif
