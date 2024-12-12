
enum threadstate {THREAD_FREE , THREAD_RUNNABLE , THREAD_RUNNING , THREAD_JOIN };

struct thread
{
  struct spinlock lock;
  enum threadstate state;
  struct trapframe *trapframe;
  uint id;
  uint join;
};