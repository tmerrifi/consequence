
#ifndef LOGICAL_CLOCK_H
#define LOGICAL_CLOCK_H

/*

  Copyright (c) 2012-15 Tim Merrifield, University of Illinois at Chicago


  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "asm/processor.h"
#include "asm/cmpxchg.h"
#include "bounded_tso.h"

//whats the max overflow period
#define MAX_CLOCK_SAMPLE_PERIOD 200000

//whats the minimum overflow period
#define MIN_CLOCK_SAMPLE_PERIOD 20000

//The value bits in the counter...this is very much model specific
#define X86_CNT_VAL_BITS 48

#define X86_CNT_VAL_MASK (1ULL << X86_CNT_VAL_BITS) - 1


#define SYNC_CLOCKS_INTERVAL 1000000ULL

#define SYNC_CLOCKS_INTERVAL_WAIT_RANGE 5000

#define SYNC_CLOCKS_MAX_WAIT 10000

#define USE_SYNC_POINT 1

//****functions for periodically syncing the clocks of running threads*******
#ifdef USE_SYNC_POINT

#define __get_sync_point_hit(ticks) (SYNC_CLOCKS_INTERVAL*(ticks/SYNC_CLOCKS_INTERVAL))

//after getting an NMI...should we sync the clocks?
static inline int logical_clock_should_sync_clocks(struct task_clock_group_info * group_info, int tid){
    uint64_t local_clock_ticks=__get_clock_ticks(group_info, tid);
    //which clock val did we hit
    uint64_t clock_val_hit=__get_sync_point_hit(local_clock_ticks);
    //make sure we actually arrived at a point where we want to sync. We check...
    //(1) did we hit here already? and (2) are we within the range to do a sync
    if (clock_val_hit > 0 &&
        group_info->clocks[__current_tid()].local_sync_barrier_clock < clock_val_hit &&
        local_clock_ticks >= clock_val_hit &&                                           
        local_clock_ticks < ((clock_val_hit)+SYNC_CLOCKS_INTERVAL_WAIT_RANGE)){
        //set the local barrier as we've now hit it...don't want to hit it again
        group_info->clocks[__current_tid()].local_sync_barrier_clock=clock_val_hit;
        return 1;
    }
    return 0;
}

//make sure we set the period so that we actually synchronize
static inline uint64_t __target_sync_point(uint64_t ticks, uint64_t period){
    uint64_t next_sync_point = ((ticks/SYNC_CLOCKS_INTERVAL + 1)*SYNC_CLOCKS_INTERVAL);
    //use SYNC_CLOCKS_INTERVAL_WAIT_RANGE to ensure that we don't set the period
    //to be just before the next sync point
    if (ticks+period >= (next_sync_point-SYNC_CLOCKS_INTERVAL_WAIT_RANGE) ){
        return (next_sync_point - ticks) + IMPRECISE_OVERFLOW_BUFFER;
    }
    return period;
}

//wait at the sync point, return true if we actually synchronized with others
static inline int logical_clock_sync_point_arrive(struct task_clock_group_info * group_info){
    //get the sync point we just hit
    uint64_t sync_point=__get_sync_point_hit(__get_clock_ticks(group_info, __current_tid()));
    //what's the current global sync point
    uint64_t current_last_global=group_info->global_sync_barrier_clock;
    int i=0;
    if (current_last_global < sync_point &&
        cmpxchg(&group_info->global_sync_barrier_clock, current_last_global, sync_point)==current_last_global){
        //first to arrive at this sync point
        for(i=0;i<SYNC_CLOCKS_MAX_WAIT;i++){cpu_relax();}
        //now lets increment it to "close" the barrier
        cmpxchg(&group_info->global_sync_barrier_clock, sync_point, sync_point+1);
        return SYNC_CLOCKS_MAX_WAIT + 1;
    }
    //here we wait for the first thread to release us (or we overstay our welcome)
    while(group_info->global_sync_barrier_clock == sync_point && i++ < SYNC_CLOCKS_MAX_WAIT){
        cpu_relax();
    }
    //we return i so that if we actually waited around we return true
    return i;
}

#else

static inline int logical_clock_should_sync_clocks(struct task_clock_group_info * group_info, int tid){
    return 0;
}

static inline uint64_t __target_sync_point(uint64_t ticks, uint64_t period){
    return period;
}

static inline void logical_clock_sync_point_arrive(struct task_clock_group_info * group_info){
    return 0;
}

#endif
/*******end of syncing functions*****/

static inline void logical_clock_update_clock_ticks(struct task_clock_group_info * group_info, int tid){
    unsigned long rawcount = local64_read(&group_info->clocks[tid].event->count); 
    __inc_clock_ticks(group_info, tid, rawcount);
    local64_set(&group_info->clocks[tid].event->count, 0);
}

//utility function to read the performance counter
static inline void __read_performance_counter(struct hw_perf_event * hwc, uint64_t * _new_raw_count, uint64_t * _prev_raw_count, uint64_t * _new_pmc){
    uint64_t new_raw_count, prev_raw_count, new_pmc;

    DECLARE_ARGS(val, low, high);

 again:    
    //read the raw counter
    rdmsrl(hwc->event_base + hwc->idx, new_raw_count);

    asm volatile("rdpmc" : EAX_EDX_RET(val, low, high) : "c" (hwc->idx));
    new_pmc=EAX_EDX_VAL(val, low, high);
    

    //get previous count
    prev_raw_count = local64_read(&hwc->prev_count);
    //in case an NMI fires while we're doing this...unlikely but who knows?!
    if (local64_cmpxchg(&hwc->prev_count, prev_raw_count, new_raw_count) != prev_raw_count){
        goto again;
    }
    //set the arguments to the values read
    *_new_raw_count=new_raw_count;
    *_prev_raw_count=prev_raw_count;
    *_new_pmc=new_pmc;
}

static inline void logical_clock_read_clock_and_update(struct task_clock_group_info * group_info, int id, bool turn_off_counter){
    uint64_t new_raw_count, prev_raw_count, new_pmc;
    int64_t delta;
    //for our version of perf counters (v3 for Intel) this works...probably not for anything else
    int shift = 64 - X86_CNT_VAL_BITS;
    struct hw_perf_event * hwc = &group_info->clocks[id].event->hw;

    //lets check to make sure the counter is currently on, otherwise we'll still read the counter but throw away the ticks we accrued
    int counter_was_on = __tick_counter_is_running(group_info);
    //read the counters using rdmsr
    __read_performance_counter(hwc, &new_raw_count, &prev_raw_count, &new_pmc);
    //if this succeeds, then its safe to turn off the tick_counter...meaning we no longer do work inside the overflow handler.
    //Even if an NMI beats us to it...it won't have any work to do since prev_raw_count==new_raw_count after the cmpxchg
    if (turn_off_counter){
        __tick_counter_turn_off(group_info);
    }
    //compute how much the counter has counted. The shift is used since only the first N (currently hard-coded to 48) bits matter
    delta = (new_raw_count << shift) - (prev_raw_count << shift);
    //shift it back to get the actually correct number
    delta >>= shift;
    //now add the event count in...why you may ask...Because the event's count field may have ticks we missed...I believe this is primarily
    //due to context switches.
    delta+=local64_read(&group_info->clocks[id].event->count);
    local64_set(&group_info->clocks[id].event->count, 0);
    if (counter_was_on){
        //add it to our current clock
        __inc_clock_ticks(group_info, id, delta);
    }
}

static inline void logical_clock_reset_current_ticks(struct task_clock_group_info * group_info, int id){
    uint64_t new_raw_count, prev_raw_count, new_pmc;
    //we can just read the counter...since that will effectively reset it
    __read_performance_counter(&group_info->clocks[id].event->hw, &new_raw_count, &prev_raw_count, &new_pmc);
    //reset the event's counter to 0
    local64_set(&group_info->clocks[id].event->count, 0);
}

static inline void logical_clock_set_perf_counter_max(struct task_clock_group_info * group_info, int id){
    struct hw_perf_event * hwc = &group_info->clocks[id].event->hw;
    int64_t val = (1ULL << 31) - 1;
    wrmsrl(hwc->event_base + hwc->idx, ((uint64_t)(-val) & X86_CNT_VAL_MASK));
    wrmsrl(hwc->event_base + hwc->idx, ((uint64_t)(-val) & X86_CNT_VAL_MASK));
    
    local64_set(&hwc->period_left, val);
    group_info->clocks[id].event->hw.sample_period=val;
    local64_set(&group_info->clocks[id].event->count, 0);
    local64_set(&hwc->prev_count, (u64)-val);

    //what did we set the counter to?
    group_info->clocks[id].event->last_written = (uint64_t)(-val);
}

static inline void logical_clock_set_perf_counter(struct task_clock_group_info * group_info, int id){
    struct hw_perf_event * hwc = &group_info->clocks[id].event->hw;
    int64_t val = group_info->clocks[id].event->hw.sample_period;
    wrmsrl(hwc->event_base + hwc->idx, ((uint64_t)(-val) & X86_CNT_VAL_MASK));
    wrmsrl(hwc->event_base + hwc->idx, ((uint64_t)(-val) & X86_CNT_VAL_MASK));
    
    local64_set(&hwc->period_left, val);
    local64_set(&group_info->clocks[id].event->count, 0);
    local64_set(&hwc->prev_count, (u64)-val);
    //what did we set the counter to?
    group_info->clocks[id].event->last_written = (uint64_t)(-val);
}


static inline void logical_clock_update_overflow_period(struct task_clock_group_info * group_info, int id){
    uint64_t myclock;
#ifdef USE_ADAPTIVE_OVERFLOW_PERIOD
    uint64_t lowest_waiting_tid_clock, new_sample_period;
    int32_t lowest_waiting_tid=0;

    //only do this if the remaining ticks is very low, just in case we ended up here "early"
    if (local64_read(&group_info->clocks[id].event->hw.period_left) < -(MIN_CLOCK_SAMPLE_PERIOD/2)){
        myclock = __get_clock_ticks(group_info,id);
        if (__single_stepping_on(group_info, id) || __hit_bounded_fence()){
            //this shouldn't matter...as we're single stepping now or about to end a bounded chunk. Just set it to something that won't overflow
            //during the single step
            group_info->clocks[id].event->hw.sample_period = IMPRECISE_BOUNDED_CHUNK_SIZE;
        }
        else{
            new_sample_period=group_info->clocks[id].event->hw.sample_period+10000;
            lowest_waiting_tid = __search_for_lowest_waiting_exclude_current(group_info, id);
            if (lowest_waiting_tid>=0){
                lowest_waiting_tid_clock = __get_clock_ticks(group_info,lowest_waiting_tid);
                //if there is a waiting thread, and its clock is larger than ours, stop when we get there
                if (lowest_waiting_tid_clock > myclock){
                    new_sample_period=__max(lowest_waiting_tid_clock - myclock + 1000, MIN_CLOCK_SAMPLE_PERIOD);
                }
            }
            group_info->clocks[id].event->hw.sample_period=__target_sync_point(myclock,
                                                                               __min(__bound_overflow_period(group_info, id, new_sample_period), MAX_CLOCK_SAMPLE_PERIOD));
        }
        local64_set(&group_info->clocks[id].event->hw.period_left, 0);
    }

#endif
}

static inline void logical_clock_reset_overflow_period(struct task_clock_group_info * group_info, int id){
#ifdef USE_ADAPTIVE_OVERFLOW_PERIOD
    group_info->clocks[current->task_clock.tid].event->hw.sample_period=MIN_CLOCK_SAMPLE_PERIOD;
    local64_set(&group_info->clocks[current->task_clock.tid].event->hw.period_left,0);
#endif
}




#endif
