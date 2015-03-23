#ifndef _STATS_H_
#define _STATS_H_

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <map>

#define TOTAL_PIDS 100000

struct stats_struct{
  unsigned long total_time;
  unsigned long total_ops;
  struct timespec time_start; 
  struct timespec time_end; 
  
};

static long _time_util_time_diff(struct timespec * start, struct timespec * end){
  unsigned long start_total = start->tv_sec * 1000000 + (start->tv_nsec/1000);
  unsigned long end_total = end->tv_sec * 1000000 + (end->tv_nsec/1000);
  return (end_total - start_total);
}

class commit_stats {

 private:
  struct stats_struct per_pid_stats[TOTAL_PIDS];

 public:

  stats_struct * stats;
  int printed;

  commit_stats(){
    stats=(stats_struct *)malloc(sizeof(stats_struct));
    memset(stats,0,sizeof(stats_struct));
    memset(per_pid_stats,0,sizeof(struct stats_struct)*(TOTAL_PIDS));
    printed=0;
  }

  void stats_inc(){
    stats->total_ops++;
  }

  void stats_pid_register(){

  }

  void stats_pid_start(){
    int pid=getpid();
    clock_gettime(CLOCK_MONOTONIC, &(per_pid_stats[pid].time_start));
  }

  void stats_pid_inc(){
    int pid=getpid();
    per_pid_stats[pid].total_ops++;
  }

  unsigned long stats_pid_aggregate(){
    unsigned long total_time=0;
    int threads=0;
    for(int i=0;i<TOTAL_PIDS;++i){
      total_time+=per_pid_stats[i].total_time;
    }
    return total_time;
  }

  int stats_get_pid_ops(){
    int pid=getpid();
    return per_pid_stats[pid].total_ops;
  }

  int stats_get_pid_time(){
    int pid=getpid();
    return per_pid_stats[pid].total_time;
  }
  
  void stats_set_pid_time(int t){
    int pid=getpid();
    per_pid_stats[pid].total_time=t;
  }


  void stats_get_pid_print_end(string message){
    int pid=getpid();
    cout << message << " pid: " << pid << " end " << per_pid_stats[pid].time_end.tv_sec << " " << per_pid_stats[pid].time_end.tv_nsec << endl;
  }

  void stats_get_pid_print_start(string message){
    int pid=getpid();

    cout << message << " pid: " << pid << " start " << per_pid_stats[pid].time_end.tv_sec << " " << per_pid_stats[pid].time_start.tv_nsec << endl;
  }

  int stats_pid_end(){
    int pid=getpid();
    clock_gettime(CLOCK_MONOTONIC,  &(per_pid_stats[pid].time_end));
    per_pid_stats[pid].total_ops++;
    int diff=_time_util_time_diff( &(per_pid_stats[pid].time_start),  &(per_pid_stats[pid].time_end));
    per_pid_stats[pid].total_time+=diff;
    return diff;
  }

  void stats_pid_print_time(string message){
    int pid=getpid();
    cout << message << " pid: " << pid << " time: " << per_pid_stats[pid].total_time << " us" << endl;
  }

   void stats_pid_print_avg(string message){
    int pid=getpid();
    cout << message << " pid: " << pid << " avg time: " << per_pid_stats[pid].total_time/per_pid_stats[pid].total_ops << " us" << endl;
  }

  void stats_pid_print_ops(string message){
    int pid=getpid();
    cout << message << " pid: " << pid << " ops: " << per_pid_stats[pid].total_ops << endl;
  }

  void stats_start(){
    int pid=getpid();
    clock_gettime(CLOCK_MONOTONIC, &stats->time_start);
  }

  int stats_end(){
    int pid=getpid();
    clock_gettime(CLOCK_MONOTONIC, &stats->time_end);
    int diff = _time_util_time_diff(&stats->time_start, &stats->time_end);
    stats->total_time+=diff;
    return diff;
  }

  void stats_print_ops(string custom_message){
    cout << custom_message << " count: " << stats->total_ops << endl;
  }
  
  void stats_print_time(string custom_message){
    cout << custom_message << " time: " << stats->total_time << " us" << endl;
  }

};

#endif
