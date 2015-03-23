#include "xrun.h"

size_t xrun::_master_thread_id;
size_t xrun::_thread_index;
struct timespec xrun::fence_start;
struct timespec xrun::fence_end;
int xrun::fence_total_us;
struct timespec xrun::token_start;
struct timespec xrun::token_end;
int xrun::token_total_us;
volatile bool xrun::_initialized = false;
volatile bool xrun::_protection_enabled = false;
size_t xrun::_children_threads_count = 0;
size_t xrun::_lock_count = 0;
bool xrun::_token_holding = false;
int xrun::free_count=0;
int xrun::commit_count=0;
struct timespec xrun::ts1;
struct timespec xrun::ts2;
int xrun::tx_coarsening_counter;
int xrun::tx_consecutively_coarsened;
int xrun::tx_current_coarsening_level;
int xrun::sleep_count;
bool xrun::is_sleeping;
bool xrun::tx_monitor_next;
bool xrun::debugged;
