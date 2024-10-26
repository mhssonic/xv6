#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/repstruct.h"


void print_reports(struct report_traps *cp);
void create_childeren();

int main(int argc, char *argv[]) {
  struct report_traps* reports = malloc(sizeof(*reports));
  int currect_pid = getpid();
  if (currect_pid < 0) {
    exit(1);
  }
  create_childeren();
  get_log(currect_pid, reports);
  print_reports(reports);
  exit(0);
}

void print_reports(struct report_traps *cp) {
    printf("number of reports: %d\n", cp->count);
    printf("PID\tPNAME\tscause\t\tspec\tstval\n");
    for (int i = 0; i < cp->count; i++) {
        struct trap_log *report = &cp->reports[i];
        printf("%d\t%s\t%ld\t\t%ld\t%ld\n", report->pid, report->pname, report->scause, report->sepc, report->stval);
    }
}



void generate_illegal_access() {
    printf("Generating illegal memory access trap...\n");
    int *ptr = (int *)0x0; 
    *ptr = 123;            
}


void create_childeren(){
  int pid = fork();
  if(pid < 0){
    return;
  }
  if(pid > 0){
    sleep(6);
    return;
  }
  pid = fork();
  if (pid > 0)
    sleep(2);
  pid = fork();
  if (pid > 0)
    sleep(2);
  generate_illegal_access();
  exit(0);
}