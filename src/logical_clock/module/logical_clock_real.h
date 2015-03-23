
#ifndef LOGICAL_REAL_H
#define LOGICAL_REAL_H

#include "time_keeping.h"

#define logical_clock_update_clock_ticks(group_info, tid)                           \
    struct timespec currentTime; \
    getrawmonotonic(&currentTime); \
    unsigned long us = __elapsed_time_us(&__get_last_read(group_info), &currentTime) + 1; \
    __inc_clock_ticks(group_info, __current_tid(), us);                 \
    getrawmonotonic(&__get_last_read(group_info));

static inline void logical_clock_init(struct task_clock_group_info * group_info, int id){
    getrawmonotonic(&__get_last_read(group_info));
}

static inline void logical_clock_read_clock_and_update(struct task_clock_group_info * group_info, int id){
    logical_clock_update_clock_ticks(group_info, id);
    //let userspace see it
    current->task_clock.user_status->ticks=__get_clock_ticks(group_info, current->task_clock.tid);

}

//happens on start
static inline void logical_clock_reset_current_ticks(struct task_clock_group_info * group_info, int id){
    getrawmonotonic(&__get_last_read(group_info));
}

static inline void logical_clock_update_overflow_period(struct task_clock_group_info * group_info, int id){}

static inline void logical_clock_reset_overflow_period(struct task_clock_group_info * group_info, int id){}




#endif
