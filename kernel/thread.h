
enum threadstate {THREAD_FREE , THREAD_RUNNABLE , THREAD_RUNNING , THREAD_JOIN };

struct thread
{
  enum threadstate state;
  struct trapframe *trapframe;
  uint id;
  uint join;
};
