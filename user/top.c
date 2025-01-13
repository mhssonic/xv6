#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/mystruct.h"

int main(){
    for(int i = 0; i<5 ; i++){
        fork();
    }
    top();
    return 0;
}