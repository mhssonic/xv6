#ifndef REPSTRUCT_H
#define REPSTRUCT_H

#define MAX_REPORT_BUFFER_SIZE 10

struct trap_log{
  int pid;
  char pname[16];
  int parents[4];
  uint64 sepc;
  uint64 scause;
  uint64 stval;
};

struct report_traps{
  int count;
  struct trap_log reports[MAX_REPORT_BUFFER_SIZE];
};

#endif // REPSTRUCT_H