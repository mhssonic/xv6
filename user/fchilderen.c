#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/mystruct.h"


void print_child_processes(struct child_proccesses *cp);
void create_childeren();

int main(int argc, char *argv[]) {
  struct child_proccesses* childeren = malloc(sizeof(*childeren));
  // if(argc < 2){
  //   fprintf(2, "usage: kill pid...\n");
  //   exit(1);
  // }
  // get_child(atoi(argv[1]), childeren);
  int currect_pid = getpid();
  if (currect_pid < 0) {
    exit(1);
  }
  create_childeren();
  get_child(currect_pid, childeren);
  print_child_processes(childeren);
  exit(0);
}

void print_child_processes(struct child_proccesses *cp) {
    printf("number of child: %d\n", cp->count);
    printf("PID\tPPID\tSTATE\t\tNAME\n");
    // Iterate over the array of child processes and print each one
    for (int i = 0; i < cp->count; i++) {
        struct proc_info *proc = &cp->proccesses[i];

        // Print PID and PPID
        printf("%d\t%d\t", proc->pid, proc->ppid);

        // Print the state (convert enum procstate to string)
        switch (proc->state) {
            case UNUSED:
                printf("unused\t\t");
                break;
            case USED:
                printf("used\t\t");
                break;
            case SLEEPING:
                printf("sleep\t\t");
                break;
            case RUNNABLE:
                printf("runnable\t");
                break;
            case RUNNING:
                printf("running\t\t");
                break;
            case ZOMBIE:
                printf("zombie\t\t");
                break;
            default:
                printf("unknown\t\t");
                break;
        }

        // Print the process name
        printf("%s\n", proc->name);
    }
}

void create_childeren(){
  int pid = fork();
  if(pid < 0){
    return;
  }
  if(pid > 0){
    sleep(10);
    return;
  }
  fork();
  fork();
  // printf("child is sleeping\n");
  sleep(5000000);
  printf("child is exiting\n");
  exit(0);
}