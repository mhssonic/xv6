
enum threadstate {THREAD_FREE , THREAD_RUNNABLE , THREAD_RUNNING , THREAD_JOIN, THREAD_ZOMBIE };

struct thread
{
  struct spinlock lock;
  enum threadstate state;
  struct trapframe *trapframe;
  uint id;
  uint join;
};