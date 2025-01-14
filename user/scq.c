#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/mystruct.h"
#include "user/user.h"

int main(){
    set_cpu_quota(1,10);
}