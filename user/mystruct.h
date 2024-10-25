enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
struct proc_info {
  enum procstate state;        // Process state
  int pid;                     // Process ID
  int ppid;         // Parent process ID
  char name[16];               // Process name (debugging)
};

struct child_proccesses {
  int count;                     // Process ID
//   int ppid;         // Parent process ID
  struct proc_info proccesses[64];               // Process name (debugging)
};