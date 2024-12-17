#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



volatile int a = 2, b = 0, c = 0;

void* my_thread(void *arg) {
    printf("im here you mother fuckers\n");
    int *number = arg;
    for (int i = 0; i < 100; ++i) {
        (*number)++;

        if (number == &a) {
            printf("thread a: %d\n", *number);
        } else if (number == &b) {
            printf("thread b: %d\n", *number);
        } else {
            printf("thread c: %d\n", *number);
        }
    }
    return 0;
}

void god(void *arg) {
    printf("im here you mother fuckers\n");
   

    int *number = arg;
    printf("number is %d, %p\n", *number, number);
    // for (int i = 0; i < 100; ++i) {
    //     (*number)++;

    //     if (number == &a) {
    //         printf("thread a: %d\n", *number);
    //     } else if (number == &b) {
    //         printf("thread b: %d\n", *number);
    //     } else {
    //         printf("thread c: %d\n", *number);
    //     }
    // }
    return;
}


int main(int argc, char *argv[]) {
    if (argc == 42) my_thread(0);
    printf("%p \n", (void *)((volatile void *)&my_thread));
    printf("%p \n", (void *)((volatile void *)my_thread));
    printf("%p \n", (void *)((volatile void *)&god));
    printf("%p \n", (void *)((volatile void *)god));

    // sleep(300);
    int ta = create_thread(&god, (void *)&a);
    // int tb = create_thread(&god, (void *)&b);
    // int tc = create_thread(&god, (void *)&c);

    // sleep(100);

    join_thread(ta);
    printf("im here you mf\n");
    // join_thread(tb);
    // join_thread(tc);
    // sleep(10);
    exit(0);
}
