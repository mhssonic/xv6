#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "user/mystruct.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];
struct {
  struct trap_log reports[MAX_REPORT_BUFFER_SIZE];
  int numberOfReport;
  int firstReport;
} _internal_report_list;


struct proc *initproc;


int nextpid = 1;
struct spinlock pid_lock;

int nexttid = 1;
struct spinlock tid_lock;


extern void forkret(void);
static void freethread(struct thread *t);
static void freeproc(struct proc *p);
void get_child_reverse(int, struct proc_info[], int*);
static void freethread(struct thread *t);
void sort_processes(struct proc_info *processes, int num_processes);


extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;
struct spinlock join_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  struct thread *t;
  
  //initlock(&proc , "proc");
  initlock(&pid_lock, "nextpid");
  initlock(&tid_lock, "nexttid");
  initlock(&wait_lock, "wait_lock");
  initlock(&join_lock, "join_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      for(t = p->threads; t < &p->threads[MAX_THREAD]; t++)
           initlock(&t->lock, "thread");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}


int
alloctid()
{
  int tid;
  
  acquire(&tid_lock);
  tid = nexttid;
  nexttid = nexttid + 1;
  release(&tid_lock);

  return tid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  struct thread *t;
  for (t = p->threads; t < &p->threads[MAX_THREAD]; t++){
    if((t->trapframe = (struct trapframe *)kalloc()) == 0){
      freethread(t);
      freeproc(p);
      release(&p->lock);
      return 0;
    }

    //TODO safe remove it
    t->id = alloctid();
  }
  p->currenct_thread = p->threads;
  // p->trapframe = p->currenct_thread->trapframe;



  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
    // // Allocate a cpu usage.
  if((p->usage = (struct cpu_usage_info *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  memset(p->usage, 0, sizeof(p->usage));
  p->usage->quota = 2; 

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}


static void
freethread(struct thread *t)
{
  memset(t->trapframe, 0, sizeof(t->trapframe));
  t->id = 0;
  t->state = THREAD_FREE;
  t->join = 0; 
}

static struct thread*
allocthread(void)
{
  struct thread *t;
  struct proc *p = myproc();
  
  for(t = p->threads; t < &p->threads[MAX_THREAD]; t++) {
    acquire(&p->lock);
    if(t->state == THREAD_FREE) {
      goto found;
    } else {
      release(&p->lock); 
    }
  }
  return 0;

found:
  t->id = alloctid();
  t->state = THREAD_RUNNABLE;

  // Allocate a trapframe page.
  if((t->trapframe = (struct trapframe *)kalloc()) == 0){
    freethread(t);
    release(&p->lock);
    return 0;
  }

  return t;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  p->trapframe = 0;
  struct thread *t;
  for(t = p->threads; t < &p->threads[MAX_THREAD]; t++)
    freethread(t);
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  // printf("4\n");
  p->state = RUNNABLE;
  p->currenct_thread->state = THREAD_RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  struct thread *nt, *ot;
  for (nt = np->threads, ot = p->threads; nt < &np->threads[MAX_THREAD]; nt++, ot++) {
    //TODO its wrong and broken
    nt->join = ot->join;
    nt->state = ot->state;
    *(nt->trapframe) = *(ot->trapframe);
    if (p->currenct_thread == ot)
      np->currenct_thread = nt;
  }
  // np->trapframe = np->currenct_thread->trapframe;


  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // *(np->usage) = *(p->usage);

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;
  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  acquire(&np->currenct_thread->lock);
  np->currenct_thread->state = THREAD_RUNNABLE;
  release(&np->currenct_thread->lock);
  release(&np->lock);
  return pid;
}

int
create_thread(void *(*start_routine)(void*), void *arg)
{
  int tid;
  struct thread *nt;
  struct proc *p = myproc();
  uint64 stack_top;

  if((nt = allocthread()) == 0){
    return -1;
  }
  memmove(nt->trapframe, p->trapframe, sizeof (struct trapframe));

  // copy saved user registers.
  // *(nt->trapframe) = *(p->trapframe);

  // void *kstack;
  // // Allocate stack for the thread
  // if ((kstack = kalloc()) == 0) {
  //     freethread(nt);
  //     release(&p->lock);
  //     return 0;
  // }

  stack_top = uvmalloc(p->pagetable, p->sz,p->sz + TREADSZ, PTE_U | PTE_W);
  if (stack_top == 0) {
      intr_on();
      panic("stack alloc failed");
      return -1;
  }
  p->sz += TREADSZ;


  nt->trapframe->sp = (uint64)stack_top; // Top of stack
  // nt->trapframe->kernel_sp = (uint64)kstack;
  nt->trapframe->ra = -1;
  tid = nt->id;
  nt->state = THREAD_RUNNABLE;
  nt->trapframe->epc = (uint64)start_routine;
  nt->trapframe->a0 = (uint64)arg;

  release(&p->lock);
  return tid;
}

int 
join_thread(int tid){
  struct thread *t;
  struct proc *p = myproc();

  acquire(&p->lock);
  struct thread *ct = p->currenct_thread;
  release(&p->lock);
  for (t = p->threads; t < &p->threads[MAX_THREAD]; t++)
  {
    if(t->id == tid){
      while(1){
        // acquire(&t->lock);
        if(t->state == THREAD_JOIN){
          printf("im here wtf should i do \n");
          p = myproc();
          acquire(&p->lock);
          p->state = RUNNABLE;
          freethread(t);
          // release(&t->lock);
          release(&p->lock);
          return 0;
        }else if(ct->state != THREAD_WAIT && (t->state == THREAD_RUNNING || t->state == THREAD_RUNNABLE)){
          p = myproc();
          acquire(&p->lock);
          p->state = RUNNABLE;
          ct->join = tid;
          ct->state = THREAD_WAIT;

          // release(&t->lock);
          acquire(&ct->lock);
          sched();
          release(&p->currenct_thread->lock);
          release(&p->lock);
          return 0;
        }
      }
    }
  }

  return -1;
}

int 
stop_thread(int tid){
  struct thread *t, *ct = 0;
  struct proc *p = myproc();
  acquire(&p->lock);
  if (tid == -1){
    if(p->currenct_thread->state == THREAD_RUNNING){
        acquire(&p->currenct_thread->lock);
        ct = p->currenct_thread;
    }else{
      release(&p->lock);
      return -1;
    }
  }else{
    for (t = p->threads; t < &p->threads[MAX_THREAD]; t++){
        if(t->id == tid){
          if(t->state == THREAD_RUNNING){
            acquire(&t->lock);
            ct = t;
          }else{
            release(&p->lock);
            return -1;
          }
        }
      }
    if (ct == 0){
      release(&p->lock);
      return -1;
    }
  }
  for (t = p->threads; t < &p->threads[MAX_THREAD]; t++){
    if (t->id != ct->id){
      acquire(&t->lock);
      if(t->state == THREAD_WAIT && t->join == ct->id){
        t->state = THREAD_RUNNABLE;
        t->join = 0;
        release(&t->lock);
        if (ct->state == THREAD_RUNNING){
          freethread(ct);
          if (p->state == RUNNING)
            p->state = RUNNABLE;

          sched();
          release(&p->currenct_thread->lock);
          release(&p->lock);
        } else{
          ct->state = THREAD_JOIN;
          release(&ct->lock);
        }
        return 0;
      }
      release(&t->lock);
    }
  }
  if (ct->state == THREAD_RUNNING){
    ct->state = THREAD_JOIN;
    p->state = RUNNABLE;
    sched();
    release(&p->currenct_thread->lock);
  } else{
    ct->state = THREAD_JOIN;
    release(&ct->lock);
  }

  
  release(&p->lock);
  return 0;
}


// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);
  acquire(&p->currenct_thread->lock);

  p->xstate = status;
  p->state = ZOMBIE;
  p->currenct_thread->state = THREAD_FREE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();
  struct thread *pt;

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          for(pt = pp->threads; pt < &pp->threads[MAX_THREAD]; pt++)
            freethread(pt);
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct thread *t;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    intr_on();

    int found = 0;
    int qeuee = 0;
    for(;;){
      for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == RUNNABLE && (qeuee || p->usage->sum_of_ticks < p->usage->quota)) {
          for(t = p->threads; t < &p->threads[MAX_THREAD]; t++) {
            // printf("trying to run a thread\n");
            acquire(&t->lock);

            if(t->state == THREAD_RUNNABLE) {
              // Switch to chosen process.  It is the process's job
              // to release its lock and then reacquire it
              // before jumping back to us.
              p->state = RUNNING;
              t->state = THREAD_RUNNING;
              c->proc = p;
              if (p->currenct_thread->state != THREAD_FREE)
                memmove(p->currenct_thread->trapframe,p->trapframe,sizeof (struct trapframe));
              p->currenct_thread = t;
              memmove(p->trapframe,t->trapframe,sizeof (struct trapframe));

              p->usage->last_calculated_tick = ticks;
              if(p->usage->start_tick == 0)
                p->usage->start_tick = ticks;
              if (p->pid == 3)
                p->usage->start_tick = 3;
              swtch(&c->context, &p->context);
              // if (t->state != THREAD_FREE)
              //   memmove(t->trapframe,p->trapframe,sizeof (struct trapframe));

              // Process is done running for now.
              // It should have changed its p->state before coming back.
              c->proc = 0;
              found = 1;
            }
            release(&t->lock);
          }
        }
        release(&p->lock);
      }
      if(!found && qeuee)
        break;
      if(!found)
        qeuee = 1;
    }
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();
  // printf("im here to make the job done %d\n", p->pid);

  if(!holding(&p->lock))
    panic("sched p->lock");
  //TODO lock this shit
  if(!holding(&p->currenct_thread->lock))
    panic("sched currenct_thread->lock");
  if(mycpu()->noff != 2)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;

  // printf("sched sum: %d %d\n", p->usage->sum_of_ticks, p->pid);
  // printf("sched ticks: %d %d\n", ticks, p->pid);
  p->usage->sum_of_ticks += ticks - p->usage->last_calculated_tick;
  p->usage->last_calculated_tick = ticks;
  /*
  if(p->usage->sum_of_ticks > p->usage->quota){
    if (p->usage->sum_of_ticks > p->usage->quota) {
    struct proc temp = *p; 
    int i;
  
    for (i = p->pid; i < NPROC - 1; i++) {
      //acquire(proc);
        proc[i] = proc[i - 1];
      //release(proc);  
    }

    proc[NPROC - 1] = temp;
    }
  }
  */
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  acquire(&p->currenct_thread->lock);
  p->state = RUNNABLE;
  p->currenct_thread->state = THREAD_RUNNABLE;
  sched();
  release(&p->currenct_thread->lock);
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->currenct_thread->lock);
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  acquire(&p->currenct_thread->lock);
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->currenct_thread->state = THREAD_WAIT;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->currenct_thread->lock);
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      //TODO lock
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        p->currenct_thread->state = THREAD_RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
        p->currenct_thread->state = THREAD_RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

struct child_proccesses* get_child(int pid)
{
  struct child_proccesses *childeren = (struct child_proccesses*)kalloc();
  childeren->count = 0;
  get_child_reverse(pid, childeren->proccesses, &childeren->count);
  return childeren;
} 

void get_child_reverse(int pid, struct proc_info childeren[], int* last)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    // printf("here on %d\n", p->pid);
    acquire(&p->lock);
    if(p->parent != NULL && p->parent->pid == pid){
      struct proc_info *child = &childeren[*last]; 
      (*last)++;
      child->pid = p->pid;
      child->ppid = pid;
      child->state = p->state;
      strncpy(child->name, p->name, 16);
      release(&p->lock);
      get_child_reverse(child->pid, childeren, last);
    } else {
      release(&p->lock);
    }
  }
  return;
} 

void log_trap(struct proc* p, uint64 scause, uint64 spec, uint64 stval){
  int new_index = _internal_report_list.numberOfReport + _internal_report_list.firstReport;
  if (new_index >= MAX_REPORT_BUFFER_SIZE){
    new_index -= MAX_REPORT_BUFFER_SIZE;
    if (new_index == _internal_report_list.firstReport)
      _internal_report_list.firstReport ++;
  }
  struct trap_log *new_log = &(_internal_report_list.reports[new_index]);
  new_log->pid = p->pid;
  strncpy(new_log->pname, p->name, 16);
  new_log->scause = scause;
  new_log->sepc = spec;
  new_log->stval = stval;
  struct proc *parent = p->parent;
  for(int i = 0; i < 4; i++){
    if (parent == NULL)
      break;
    new_log->parents[i] = parent->pid;
    // printf("added parent %d\n", parent->pid);
    parent = parent->parent;
  }
  _internal_report_list.numberOfReport++;
  // printf("number of reports %d \n", _internal_report_list.numberOfReport);
  log_trap_to_file(new_log);
}


void get_log(int pid, struct report_traps* result){
  result->count = 0;
  int i = _internal_report_list.firstReport;
  printf("for pid %d \n", pid);
  for (int n = 0; n < _internal_report_list.numberOfReport; n++){
    struct trap_log log = _internal_report_list.reports[(i + n) % MAX_REPORT_BUFFER_SIZE];
    printf("%d: ", log.pid);
    for (int k = 0; k < 4; k++){
      printf("%d ", log.parents[k]);
    }
    printf("\n");
  }


  i = _internal_report_list.firstReport;
  for (int n = 0; n < _internal_report_list.numberOfReport; n++){
    struct trap_log log = _internal_report_list.reports[(i + n) % MAX_REPORT_BUFFER_SIZE];
    int not_the_father = 1;
    for (int k = 0; k < 4; k++){
      if(log.parents[k] == pid){
        not_the_father = 0;
        break;
      }
    }
    if (not_the_father)
      continue;

    result->reports[result->count] = log;
    result->count ++;
  }
  return;
}

int 
cpu_usage(int pid, struct cpu_usage_info* usage){
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      *usage = *(p->usage);
      if(p->state == RUNNING){
        usage->sum_of_ticks += ticks - usage->last_calculated_tick;
      }
      return 0;
    }
  }
  return 0;
}


int 
top(struct top* top){
  struct proc* p;
  struct proc_info* info;
  int i = 0;
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    info = &top->processes[i++];
    info->pid = p->pid;
    strncpy(info->name, p->name, 16);
    if(p->parent != 0)
      info->ppid = p->parent->pid;
    else
      info->ppid = -1;
    info->state = p->state;
    info->usage = *(p->usage);
    if(p->state == RUNNING){
      info->usage.sum_of_ticks += ticks - info->usage.last_calculated_tick;
    }
  }
  top->count = i;
  sort_processes(top->processes, i);
  return 0;
}


int 
set_cpu_quota(int pid , int quota){
  struct proc *p;
  struct proc *parent;
  struct proc *executer = myproc();

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED) 
      continue;

    if(p->pid == pid){
      parent = p;
      while(parent->pid != executer->pid && parent->parent)
        parent = parent->parent;
      if(parent->pid == executer->pid){
        p->usage->quota = quota;
        return 0;
      }
      return -1;
    } 
  }
  return -1;
}



void sort_processes(struct proc_info *processes, int num_processes) {
    int i, j, max_idx;
    struct proc_info temp;

    for (i = 0; i < num_processes - 1; i++) {
        max_idx = i;
        for (j = i + 1; j < num_processes; j++) {
            if (processes[j].usage.sum_of_ticks > processes[max_idx].usage.sum_of_ticks) {
                max_idx = j;
            }
        }

        if (max_idx != i) {
            temp = processes[i];
            processes[i] = processes[max_idx];
            processes[max_idx] = temp;
        }
    }
}