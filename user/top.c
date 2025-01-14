#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/mystruct.h"

void print_processes(struct top *top_res);
int main(){
    int a = 1;
    int fork_id[3];
    for(int i = 0; i<4 ; i++){
        if((fork_id[i] = fork()) == 0){

            for (int j = 0; j < 100000; j++)
            for (int k = 0; k < 100000; k++)
                a *= 2;
            printf("im going to sleep %d\n", i);
            sleep(100);
            exit(0);
        }
    }
    if (fork_with_deadline(40) == 0){
        for (int j = 0; j < 100000; j++)
        for (int i = 0; i < 100000; i++)
            a *= 2;
        printf("im going to sleep %d\n", -1);
        sleep(100);
        exit(0);
    }
    
    // sleep(20);
    // set_cpu_quota(fork_id[1], 20);
    // set_cpu_quota(fork_id[2], 30);
    set_cpu_quota(fork_id[3], 20);

    while(1){
        struct top* top_res = malloc(sizeof(*top_res));
        if(top(top_res) < 0){
            return -1;
        }
        print_processes(top_res);
        sleep(10);
        for (int j = 0; j < 10000; j++)
        for (int i = 0; i < 10000; i++)
            a *= 2;
    }
    return 0;
}

void print_processes(struct top *top_res) {
    printf("number of process: %d\n", top_res->count);
    printf("PID\tPPID\tSTART\tUSAGE\tQUOTA\tSTATE\t\tNAME\n");
    struct proc_info *proc;
    struct proc_info *last = &top_res->processes[top_res->count -1];
    for (proc = top_res->processes; proc <= last; proc++) {
        

        // Print PID and PPID
        printf("%d\t%d\t%d\t%d\t%d\t", proc->pid, proc->ppid, proc->usage.start_tick, proc->usage.sum_of_ticks, proc->usage.quota);
        

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