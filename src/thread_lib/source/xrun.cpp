#include "speculation.h"
#include "xrun.h"

size_t xrun::_master_thread_id;
size_t xrun::_thread_index;
volatile bool xrun::_initialized = false;
size_t xrun::_lock_count = 0;
bool xrun::_token_holding = false;
int xrun::tx_coarsening_counter;
int xrun::tx_consecutively_coarsened;
int xrun::tx_current_coarsening_level;
int xrun::sleep_count;
bool xrun::is_sleeping;
bool xrun::tx_monitor_next;
uint64_t xrun::heapVersionToWaitFor;
uint64_t xrun::globalsVersionToWaitFor;
uint64_t xrun::_last_token_release_time;
int xrun::progress;
int xrun::testing;

speculation * xrun::_speculation;
