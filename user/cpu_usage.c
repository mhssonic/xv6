#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/mystruct.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  if(argc < 2){
    fprintf(2, "usage: cpu_usage pid...\n");
    exit(1);
  }
  struct cpu_usage_info* usage = malloc(sizeof(*usage));
  cpu_usage(atoi(argv[1]), usage);
  printf("%d\n", usage->sum_of_ticks);
  exit(0);
}