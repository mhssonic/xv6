#include "kernel/param.h"
#ifndef MYSTRUCT_H
#define MYSTRUCT_H
enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct proc_info {
  enum procstate state;        // Process state
  int pid;                     // Process ID
  int ppid;         // Parent process ID
  char name[16];               // Process name (debugging)
  //struct cpu_usage_info usage; 
};

struct child_proccesses {
  int count;                     // Process ID
//   int ppid;         // Parent process ID
  struct proc_info proccesses[64];               // Process name (debugging)
};

struct cpu_usage_info {
  uint sum_of_ticks; //shows cpu usage of process 
  uint start_tick; //set at the response time
  uint quota; //how it could use?
};


struct top
{
  int count;
  struct proc_info processes[NPROC];
};




#endif // MYSTRUCT_H