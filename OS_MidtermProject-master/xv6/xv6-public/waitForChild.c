#include "types.h"
#include "stat.h"
#include "user.h"

struct timeVariables{
  int creationTime;
  int terminationTime;
  int sleepingTime; 
  int readyTime; 
  int runningTime;
  };
int allCbt = 0;
int main(void) {
   changePolicy(1);//modified xv6 algorithm

   for(int f=0; f<10;f++){
    int pid = fork();
    if (pid == 0){//if 0, then we are in the child process
        for (int i = 0; i < 1000; ++i)
		printf(1, "%d : %d \n", getpid(), i);
		//sleep(0.1);
        exit();
    } else if(pid > 0){
        struct timeVariables *tv = malloc(sizeof(struct timeVariables));
        //sleep(7);
        waitForChild(tv);
        allCbt += tv->runningTime;
        printf(1, "creationTime %d\n", tv->creationTime);
        printf(1, "terminationTime %d\n", tv->terminationTime);
        printf(1, "readyTime %d\n", tv->readyTime);
        printf(1, "sleepingTime %d\n", tv->sleepingTime);
        printf(1, "runningTime %d\n \n", tv->runningTime);
        
    }
    }
    //int average = allCbt / 10;
    printf(1, "all cbt %d average is : %d \n ", allCbt, (allCbt / 10));
    exit();
}

