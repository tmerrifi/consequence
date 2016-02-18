
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



#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/task_clock.h>
#include <linux/slab.h>
#include <linux/irq_work.h>
#include <linux/wait.h>
#include <linux/perf_event.h>
#include <linux/bitops.h>
#include <linux/vmalloc.h>
#include <linux/hardirq.h>

#include "listarray.h"
#include "bounded_tso.h"
#include "utility.h"
#include "search_entries.h"

#ifdef NO_INSTRUCTION_COUNTING
#include "logical_clock_no_ticks.h"
#else
#include "logical_clock_instruction_counting.h"
#endif

MODULE_LICENSE("GPL");


#ifdef USE_SYNC_POINT

#define DEBUG_SYNC_POINTS_NUM 256
#define DEBUG_SYNC_POINTS_THREADS 8

struct __debug_sync_points{
    uint64_t points[DEBUG_SYNC_POINTS_NUM];
    int counter;
}sync_points[DEBUG_SYNC_POINTS_THREADS];

void __add_debug_sync_points(uint64_t value, int tid){
    if (sync_points[tid].counter < DEBUG_SYNC_POINTS_NUM){
        sync_points[tid].points[sync_points[tid].counter++]=value;
    }
}


void __print_debug_sync_points(){
    int i,j;
    for (i=0;i<DEBUG_SYNC_POINTS_THREADS;i++){
        for (j=0;j<sync_points[i].counter;j++){
            printk(KERN_EMERG "t: %d %lld\n", i, sync_points[i].points[j]);
        }
    }
}

#else

void __add_debug_sync_points(uint64_t value, int tid){}


#endif

//after we execute this function, a new lowest task clock has been determined
int32_t __new_lowest(struct task_clock_group_info * group_info, int32_t tid){
  int32_t new_low=-1;
  int32_t tmp;

  if (group_info->lowest_tid == -1){
      new_low=__search_for_lowest(group_info);
  }
  //am I the current lowest?
  else if (tid==group_info->lowest_tid && ((tmp=__search_for_lowest(group_info))!=tid) ){
      //looks like things have changed...someone else is the lowest
      new_low=tmp;
  }
  //I'm not the lowest, but perhaps things have changed
  else if (tid!=group_info->lowest_tid && __clock_is_lower(group_info, tid, group_info->lowest_tid)){
    new_low=tid;
  }
  return new_low;
}

int32_t __thread_is_waiting(struct task_clock_group_info * group_info, int32_t tid){
  return (tid >= 0 && group_info->clocks[tid].waiting);
}

int8_t __is_sleeping(struct task_clock_group_info * group_info, int32_t tid){
    return (tid >= 0 && group_info->clocks[tid].sleeping);
}

void __wake_up_waiting_thread(struct task_clock_group_info * group_info, int32_t tid){
  struct perf_buffer * buffer;
  struct perf_event * event = group_info->clocks[tid].event;
  if (__is_sleeping(group_info, group_info->lowest_tid)){
      rcu_read_lock();
      buffer = rcu_dereference(event->buffer);
      atomic_set(&buffer->poll, POLL_IN);
      rcu_read_unlock();
      wake_up_all(&event->task_clock_waitq);
  }
  else{
      //set the lowest threads lowest
      group_info->user_status_arr[group_info->lowest_tid].lowest_clock=1;
  }
  group_info->user_status_arr[group_info->lowest_tid].notifying_clock=__get_clock_ticks(group_info, __current_tid());
  group_info->user_status_arr[group_info->lowest_tid].notifying_id=__current_tid();
}

void __set_new_low(struct task_clock_group_info * group_info, int32_t tid){
    group_info->lowest_tid=tid;
    group_info->lowest_ticks=__get_clock_ticks(group_info, tid);
}

    
void __task_clock_notify_waiting_threads(struct task_clock_group_info * group_info){
    unsigned long flags;
    int lowest_tid=-1;
    spin_lock(&group_info->lock);
    int new_low=__search_for_lowest_waiting(group_info);
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
    printk(KERN_EMERG "TASK CLOCK: beginning notification of %d\n", new_low);
#endif
    if (new_low>=0 && __thread_is_waiting(group_info,new_low)){
        __set_new_low(group_info,new_low);
        if (group_info->notification_needed || group_info->nmi_new_low){
            group_info->nmi_new_low=0;
            group_info->notification_needed=0;
            //the lowest must be notified
            lowest_tid = group_info->lowest_tid;
        }
    }
    spin_unlock(&group_info->lock);
    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }

}

void __task_clock_notify_waiting_threads_irq(struct irq_work * work){
    struct task_clock_group_info * group_info = container_of(work, struct task_clock_group_info, pending_work);
    __task_clock_notify_waiting_threads(group_info);
}

void task_clock_entry_overflow_update_period(struct task_clock_group_info * group_info){
    unsigned long flags;
    //if we don't want to count ticks...don't do any of this work.
    if (!__tick_counter_is_running(group_info)){
        return;
    }
    logical_clock_update_clock_ticks(group_info, __current_tid());
    logical_clock_update_overflow_period(group_info, __current_tid());
}
   

void task_clock_overflow_handler(struct task_clock_group_info * group_info, struct pt_regs *regs){
  unsigned long flags;
  int32_t new_low=-1;

  //if we don't want to count ticks...don't do any of this work.
  if (!__tick_counter_is_running(group_info)){
      return;
  }

  group_info->user_status_arr[group_info->lowest_tid].notifying_diff++;

  if (__single_stepping_on(group_info, __current_tid())){
      bounded_memory_fence_turn_on_tf(group_info, regs);
  }

  new_low=__new_lowest(group_info, current->task_clock.tid);
  
  if (new_low >= 0 && new_low != current->task_clock.tid){
      group_info->user_status_arr[new_low].lowest_clock=1;
      group_info->user_status_arr[new_low].notifying_clock=__get_clock_ticks(group_info, __current_tid());
      group_info->user_status_arr[new_low].notifying_id=__current_tid();
  }
  
  //did we hit a sync point???
  if (logical_clock_should_sync_clocks(group_info, __current_tid())){
      //if so, lets wait and sync up
      int result=logical_clock_sync_point_arrive(group_info);
      //debugging stuff
      if (result){
          __add_debug_sync_points(group_info->clocks[__current_tid()].local_sync_barrier_clock, __current_tid());
      }
  }  
}

void __set_current_thread_to_lowest(struct task_clock_group_info * group_info){
    current->task_clock.user_status->lowest_clock=1;
    //clear the notification flag
    group_info->notification_needed=0;
    //clear the state
    __clear_entry_state(group_info);
    group_info->lowest_ticks=__get_clock_ticks(group_info,__current_tid());
}

int __determine_lowest_and_notify_or_wait(struct task_clock_group_info * group_info, int user_event){

    int thread_to_wakeup=-1;
    int32_t new_low;

    //put the clock ticks into userspace no matter what
    //current->task_clock.user_status->ticks=__get_clock_ticks(group_info, current->task_clock.tid);

    //if we're the only active thread, do our work and get out
    if (__current_is_only_active_thread(group_info)){
        //set it as a single active thread
        current->task_clock.user_status->single_active_thread=1;
        __set_current_thread_to_lowest(group_info);
    }
    else{
        //we set ourselves to be waiting, this may change
        group_info->clocks[current->task_clock.tid].waiting=1;
        //figure out who the lowest clock is
        new_low=__search_for_lowest_waiting(group_info);    
        //if its not already the lowest_tid, then set it and make sure we notify
        if (new_low!=group_info->lowest_tid){
            group_info->notification_needed=1;
            group_info->lowest_tid=new_low;
        }
        //if we're the lowest, set up our state
        if (new_low >= 0 && group_info->lowest_tid == current->task_clock.tid){
            __set_current_thread_to_lowest(group_info);
        }
        else{
            //we are not the lowest clock, make sure we're all on the same page
            current->task_clock.user_status->lowest_clock=0;
            //is this thread waiting, and has the notification not been sent yet? If so, then send it
            if (__thread_is_waiting(group_info, group_info->lowest_tid) && group_info->notification_needed){
                //set the state for this thread
                group_info->nmi_new_low=0;
                group_info->notification_needed=0;
                thread_to_wakeup=group_info->lowest_tid;
                //the lowest must be notified
            }
        }
        if (group_info->lowest_tid>=0){
            //set the group-level lowest ticks
            group_info->lowest_ticks=__get_clock_ticks(group_info,group_info->lowest_tid);
        }

#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED) && defined(DEBUG_TASK_CLOCK_FINE_GRAINED)
        printk(KERN_EMERG "--------TASK CLOCK: lowest_notify_or_wait clock ticks %llu, id %d, waiting %d, lowest clock %d\n", 
               __get_clock_ticks(group_info, current->task_clock.tid), current->task_clock.tid, 
               group_info->clocks[current->task_clock.tid].waiting, current->task_clock.user_status->lowest_clock);
#endif
    }
    
    return thread_to_wakeup;
}

//userspace is disabling the clock. Perhaps they are about to start waiting to be named the lowest. In that
//case, we need to figure out if they are the lowest and let them know before they call poll
void task_clock_on_disable(struct task_clock_group_info * group_info){
    int lowest_tid=-1;
    
    //if we are in single stepping mode, we no longer need it
    if (__single_stepping_on(group_info, __current_tid())){
        end_bounded_memory_fence_early(group_info);
    }

    //if the counter is running, grab the ticks that may not have been counted by the NMI handler
    if (__tick_counter_is_running(group_info)){
        logical_clock_update_clock_ticks(group_info, current->task_clock.tid);
        //turn the counter off...
        __tick_counter_turn_off(group_info);
    }
    else{
        //clear the counter
        logical_clock_reset_current_ticks(group_info,__current_tid());
    }
    __add_lazy_ticks(group_info, current->task_clock.tid);
    
    group_info->notification_needed=1;
    spin_lock(&group_info->lock);
    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 10);
    //am I the lowest?
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
    printk(KERN_EMERG "TASK CLOCK: disabling %d...lowest is %d lowest clock is %d pid %d\n", 
           current->task_clock.tid, group_info->lowest_tid, current->task_clock.user_status->lowest_clock, current->pid);
#endif
    spin_unlock(&group_info->lock);
    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }
}

void task_clock_add_ticks(struct task_clock_group_info * group_info, int32_t ticks){
    int lowest_tid=-1;
    spin_lock(&group_info->lock);
    __inc_clock_ticks_no_chunk_add(group_info, current->task_clock.tid, ticks);
    //current->task_clock.user_status->ticks=__get_clock_ticks(group_info, current->task_clock.tid);
    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 11);
    spin_unlock(&group_info->lock);
    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }

}

void task_clock_on_enable(struct task_clock_group_info * group_info){
    int lowest_tid=-1;
    spin_lock(&group_info->lock);

    //reset the tick value now so we can figure out the length of a chunk later
    __reset_chunk_ticks(group_info, __current_tid());
    __add_lazy_ticks(group_info, current->task_clock.tid);
    //turn the counter back on...next overflow will actually be counted
    __tick_counter_turn_on(group_info);
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
    printk(KERN_EMERG "TASK CLOCK: ENABLING %d, lowest? %d\n", current->task_clock.tid, group_info->lowest_tid);
#endif
    logical_clock_reset_overflow_period(group_info, __current_tid());
    //__update_period(group_info);
    logical_clock_update_overflow_period(group_info, __current_tid());

    if (group_info->clocks[current->task_clock.tid].userspace_reading==1){
        group_info->clocks[current->task_clock.tid].userspace_reading=0;
        logical_clock_set_perf_counter(group_info, __current_tid());
    }

    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 12);
    //are we the lowest?
    if (group_info->lowest_tid==current->task_clock.tid){
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED) && defined(DEBUG_TASK_CLOCK_FINE_GRAINED)
        printk(KERN_EMERG "----TASK CLOCK: ENABLING %d and setting notification to 1 %d\n", current->task_clock.tid);
#endif
        group_info->notification_needed=1;
        group_info->nmi_new_low=0;
    }
    __clear_entry_state(group_info);
    spin_unlock(&group_info->lock);
    if (lowest_tid>=0){           
        __wake_up_waiting_thread(group_info, lowest_tid);
    }
}

void __init_task_clock_entries(struct task_clock_group_info * group_info){
  int i=0;
  for (;i<TASK_CLOCK_MAX_THREADS;++i){
    struct task_clock_entry_info * entry = &group_info->clocks[i];
    entry->initialized=0;
    entry->waiting=0;
    entry->sleeping=0;
    entry->inactive=0;
    entry->ticks=0;
  }
}

//return a pte given an address
pte_t * pte_get_entry_from_address(struct mm_struct * mm, unsigned long addr){
	
	pgd_t * pgd;
	pud_t *pud;
	pte_t * pte;
	pmd_t *pmd;

	pgd = pgd_offset(mm, addr);
	if (!pgd){
		goto error;
	}
	pud = pud_alloc(mm, pgd, addr);
	if (!pud){
		goto error;
	}
	pmd = pmd_alloc(mm, pud, addr);
	if (!pmd){
		goto error;	
	}
	pte = pte_alloc_map(mm, pmd, addr);
	if (!pte){
		goto error;
	}
	return pte;
	
	error:
		return NULL;
}

void task_clock_entry_init(struct task_clock_group_info * group_info, struct perf_event * event){
 
    pte_t * the_pte;

   if (group_info->clocks[current->task_clock.tid].initialized==0){
        group_info->clocks[current->task_clock.tid].ticks=0;
        group_info->clocks[current->task_clock.tid].base_ticks=0;
    }
    group_info->clocks[current->task_clock.tid].initialized=1;
    group_info->clocks[current->task_clock.tid].userspace_reading=0;
    group_info->clocks[current->task_clock.tid].event=event;
    group_info->clocks[current->task_clock.tid].count_ticks=0;
    group_info->clocks[current->task_clock.tid].local_sync_barrier_clock=0;
    
    __reset_chunk_ticks(group_info, __current_tid());
    __clear_entry_state(group_info);
    __tick_counter_turn_off(group_info);

    if (current->task_clock.tid==0 && group_info->user_status_arr==NULL){
        unsigned long user_page_addr = PAGE_ALIGN((unsigned long)(current->task_clock.user_status)) - PAGE_SIZE;
        //get the offset (from the start of the page) to user_stats
        unsigned int page_offset = (unsigned long)current->task_clock.user_status - user_page_addr;
        //get the physical frame
        the_pte=pte_get_entry_from_address(current->mm, user_page_addr);
        BUG_ON(the_pte==NULL);
        struct page * p = pte_page(*the_pte);
        //map it into the kernel's address space
        void * mapped_page_addr=vmap(&p, 1, VM_MAP, PAGE_KERNEL);
        group_info->user_status_arr=(struct task_clock_user_status *)(mapped_page_addr+page_offset);
    }
    current->task_clock.group_info=group_info;
    current->task_clock.user_status->hwc_idx=-1;

#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
  printk(KERN_EMERG "TASK CLOCK: INIT, tid %d event is %p....ticks are %llu \n", 
         current->task_clock.tid, event, __get_clock_ticks(group_info, current->task_clock.tid));
#endif
}

struct task_clock_group_info * task_clock_group_init(void){
  struct task_clock_group_info * group_info = kmalloc(sizeof(struct task_clock_group_info), GFP_KERNEL);
  spin_lock_init(&group_info->nmi_lock);
  spin_lock_init(&group_info->lock);
  group_info->lowest_tid=-1;
  group_info->lowest_ticks=0;
  group_info->notification_needed=1;
  group_info->nmi_new_low=0;
  group_info->user_status_arr=NULL;
  group_info->active_threads = kmalloc(sizeof(struct listarray), GFP_KERNEL);
  group_info->global_sync_barrier_clock=0;
  listarray_init(group_info->active_threads);
  __init_task_clock_entries(group_info);
  init_irq_work(&group_info->pending_work, __task_clock_notify_waiting_threads_irq);
  return group_info;
}

void task_clock_entry_halt(struct task_clock_group_info * group_info){
  int32_t lowest_tid=-1;
  //first, check if we're the lowest
  spin_lock(&group_info->lock);
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
  printk(KERN_EMERG "TASK CLOCK: halting %d\n", __current_tid());
#endif
  //make us inactive
  __mark_as_inactive(group_info, __current_tid());
  lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 13);
  //clear the waiting flag...if its set we are not really "waiting"
  group_info->clocks[__current_tid()].waiting=0;
  spin_unlock(&group_info->lock);
  if (lowest_tid>=0){
      __wake_up_waiting_thread(group_info, lowest_tid);
  }
}

void task_clock_entry_activate(struct task_clock_group_info * group_info){
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
  printk(KERN_EMERG "TASK CLOCK: activating %d\n", current->task_clock.tid);
#endif
  unsigned long flags;
  spin_lock(&group_info->lock);
  __clear_entry_state(group_info);
  //need to reset chunk ticks before calling logical_clock_update_overflow_period
  __reset_chunk_ticks(group_info, __current_tid());
  __mark_as_active(group_info, __current_tid());
  logical_clock_reset_overflow_period(group_info, __current_tid());
  //__update_period(group_info);
  logical_clock_update_overflow_period(group_info, __current_tid());
  current->task_clock.user_status->notifying_id=0;
  current->task_clock.user_status->notifying_sample=0;
  current->task_clock.user_status->notifying_diff=0;
  current->task_clock.user_status->hit_bounded_fence=0;
  current->task_clock.user_status->period_sets=0;
  
  group_info->clocks[current->task_clock.tid].userspace_reading=0;
    //if I'm the new lowest, we need to set the flag so userspace can see that that is the case
  int32_t new_low=__new_lowest(group_info, current->task_clock.tid);
  if (new_low >= 0 && new_low == current->task_clock.tid){
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
      printk(KERN_EMERG "TASK CLOCK: activated thread is the new low,  %d\n", current->task_clock.tid);
#endif
      group_info->notification_needed=0;
      group_info->lowest_tid=new_low;
      current->task_clock.user_status->lowest_clock=1;

  }
  spin_unlock(&group_info->lock);
}

void task_clock_entry_activate_other(struct task_clock_group_info * group_info, int32_t id){
    
    spin_lock(&group_info->lock);
#if defined(DEBUG_TASK_CLOCK_COARSE_GRAINED)
    printk(KERN_EMERG "TASK CLOCK: activating_other %d activating %d\n", current->task_clock.tid, id);
#endif
    if (group_info->clocks[id].initialized!=1){
        group_info->clocks[id].base_ticks=__get_clock_ticks(group_info, current->task_clock.tid) + 1;
        group_info->clocks[id].ticks=0;
    }
    group_info->clocks[id].initialized=1;
    __clear_entry_state_by_id(group_info, id);
    __mark_as_active(group_info, id);
    __reset_chunk_ticks(group_info, id);
    //if the newly activated thread is the lowest, then we need to set a flag so userspace can deal with it. Since
    //another thread may be convinced it is the lowest
    if (group_info->lowest_tid < 0 || __clock_is_lower(group_info, id, group_info->lowest_tid)){
        current->task_clock.user_status->activated_lowest=1;
    }

    spin_unlock(&group_info->lock);
}


void task_clock_entry_wait(struct task_clock_group_info * group_info){
    int lowest_tid=-1;
    spin_lock(&group_info->lock);

    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 13);
    spin_unlock(&group_info->lock);
    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }

}

void task_clock_entry_reset(struct task_clock_group_info * group_info){
    group_info->clocks[__current_tid()].initialized=0;
}

void task_clock_entry_sleep(struct task_clock_group_info * group_info){
    int lowest_tid=-1;
    spin_lock(&group_info->lock);
    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 14);
    //set ourselves to be sleeping
    if (group_info->clocks[current->task_clock.tid].waiting){
        group_info->clocks[current->task_clock.tid].sleeping=1;
    }

    spin_unlock(&group_info->lock);

    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }
}

//our thread is waking up
void task_clock_entry_woke_up(struct task_clock_group_info * group_info){
    int lowest_tid=-1;
    current->task_clock.user_status->notifying_sample=0;
    spin_lock(&group_info->lock);    
    group_info->clocks[current->task_clock.tid].sleeping=0;
    group_info->clocks[current->task_clock.tid].waiting=0;
    lowest_tid=__search_for_lowest(group_info);
    spin_unlock(&group_info->lock);
    if (lowest_tid==current->task_clock.tid){
        current->task_clock.user_status->lowest_clock=1;
    }
    else{
        current->task_clock.user_status->lowest_clock=0;
    }
    
}

//Called when the counting has finished...we don't actually stop counting, we just
//won't consider ticks that happen after this
void task_clock_entry_stop(struct task_clock_group_info * group_info){
    uint64_t new_raw_count, prev_raw_count;
    int64_t delta;
    int lowest_tid=-1;
    int profile=0;

    //if we are in single stepping mode, we no longer need it
    if (__single_stepping_on(group_info, __current_tid())){
        end_bounded_memory_fence_early(group_info);
    }
    
    //read the counter and update our clock
    logical_clock_read_clock_and_update(group_info, __current_tid(), true);
    spin_lock(&group_info->lock);

    lowest_tid=__determine_lowest_and_notify_or_wait(group_info, 787);
    spin_unlock(&group_info->lock);

    if (lowest_tid>=0){
        __wake_up_waiting_thread(group_info, lowest_tid);
    }
}

void task_clock_entry_read_clock(struct task_clock_group_info * group_info){
    //use this opportunity to clear the lazy ticks added from userspace
    __add_lazy_ticks(group_info, current->task_clock.tid);
    if (__tick_counter_is_running(group_info)){
        logical_clock_read_clock_and_update(group_info, __current_tid(), false);
    }
}

//just like regular stop, just don't try and wake any one up. Or grab the lock!
void task_clock_entry_stop_no_notify(struct task_clock_group_info * group_info){
    logical_clock_read_clock_and_update(group_info, __current_tid(), true);
}

//lets start caring about the ticks we see (again)
void task_clock_entry_start(struct task_clock_group_info * group_info){
    current->task_clock.user_status->notifying_id=0;
    current->task_clock.user_status->period_sets=0;
    current->task_clock.user_status->notifying_sample=0;
    //the clock may have continued to run...so reset the ticks we've seen
    logical_clock_reset_current_ticks(group_info,__current_tid());
    task_clock_on_enable(group_info);
}

void task_clock_entry_start_no_notify(struct task_clock_group_info * group_info){
    group_info->clocks[current->task_clock.tid].userspace_reading=1;
    current->task_clock.user_status->notifying_sample=0;
    //the clock may have continued to run...so reset the ticks we've seen
    logical_clock_reset_current_ticks(group_info,__current_tid());
    //now that its reset, lets set the counter to be VERY HIGH...if we get beat by an overflow its ok
    //because we've already reset the counter
    logical_clock_set_perf_counter_max(group_info,__current_tid());
    //turn off overflows...just in case
    __tick_counter_turn_off(group_info);
    current->task_clock.user_status->notifying_id=0;
    current->task_clock.user_status->period_sets=0;
}

int task_clock_entry_is_singlestep(struct task_clock_group_info * group_info, struct pt_regs *regs){
    return on_single_step(group_info, regs);
}

int init_module(void)
{
#ifdef USE_BOUNDED_FENCE
    BUG_ON(BOUNDED_CHUNK_SIZE<MAX_CLOCK_SAMPLE_PERIOD);
#endif
    
    task_clock_func.task_clock_overflow_handler=task_clock_overflow_handler;
    task_clock_func.task_clock_group_init=task_clock_group_init;
    task_clock_func.task_clock_entry_init=task_clock_entry_init;
    task_clock_func.task_clock_entry_activate=task_clock_entry_activate;
    task_clock_func.task_clock_entry_halt=task_clock_entry_halt;
    task_clock_func.task_clock_on_disable=task_clock_on_disable;
    task_clock_func.task_clock_on_enable=task_clock_on_enable;
    task_clock_func.task_clock_entry_activate_other=task_clock_entry_activate_other;
    task_clock_func.task_clock_entry_wait=task_clock_entry_wait;
    task_clock_func.task_clock_entry_sleep=task_clock_entry_sleep;
    task_clock_func.task_clock_overflow_update_period=task_clock_entry_overflow_update_period;
    task_clock_func.task_clock_add_ticks=task_clock_add_ticks;
    task_clock_func.task_clock_entry_woke_up=task_clock_entry_woke_up;
    task_clock_func.task_clock_debug_add_event=NULL;
    task_clock_func.task_clock_entry_stop=task_clock_entry_stop;
    task_clock_func.task_clock_entry_start=task_clock_entry_start;
    task_clock_func.task_clock_entry_reset=task_clock_entry_reset;
    task_clock_func.task_clock_entry_start_no_notify=task_clock_entry_start_no_notify;
    task_clock_func.task_clock_entry_stop_no_notify=task_clock_entry_stop_no_notify;
    task_clock_func.task_clock_entry_read_clock=task_clock_entry_read_clock;
#ifdef USE_BOUNDED_FENCE
    task_clock_func.task_clock_entry_is_singlestep=task_clock_entry_is_singlestep;
#else
    task_clock_func.task_clock_entry_is_singlestep=NULL;
#endif

#ifdef USE_SYNC_POINT
    memset(sync_points, 0, sizeof(struct __debug_sync_points)*DEBUG_SYNC_POINTS_THREADS);
#endif
    
  return 0;
}

void cleanup_module(void)
{
    int i,j;
  task_clock_func.task_clock_overflow_handler=NULL;
  task_clock_func.task_clock_group_init=NULL;
  task_clock_func.task_clock_entry_init=NULL;
  task_clock_func.task_clock_entry_activate=NULL;
  task_clock_func.task_clock_entry_halt=NULL;
  task_clock_func.task_clock_on_disable=NULL;
  task_clock_func.task_clock_on_enable=NULL;
  task_clock_func.task_clock_entry_activate_other=NULL;
  task_clock_func.task_clock_entry_wait=NULL;
  task_clock_func.task_clock_add_ticks=NULL;
  task_clock_func.task_clock_entry_woke_up=NULL;
  task_clock_func.task_clock_debug_add_event=NULL;  
  task_clock_func.task_clock_entry_stop=NULL;
  task_clock_func.task_clock_entry_start=NULL;
  task_clock_func.task_clock_entry_is_singlestep=NULL;
  task_clock_func.task_clock_entry_read_clock=NULL;

  #ifdef USE_SYNC_POINT

  #endif

  
}
