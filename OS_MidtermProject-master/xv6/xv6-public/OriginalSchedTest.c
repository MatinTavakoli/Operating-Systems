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
  
// Childrens TT and WT Time   
struct processTimeVariables{
  int pid;
  int turnAroundTime;
  int CBT;
  int waitingTime;
  };

// Childrens ATT and AWT Time
struct averageTimeVariables{
  int averageTurnAroundTime;
  int averageCBT;
  int averageWaitingTime;
  };

int main(void) {
   struct processTimeVariables ptv[10] ;
   struct averageTimeVariables atv;
   
   // Reinitialize the averageTimeVariables values
   atv.averageTurnAroundTime = 0;
   atv.averageCBT = 0;
   atv.averageWaitingTime = 0;
   
   // Change The Scheduling Algorithm To The QUANTUM 
   changePolicy(1); 
   
   // Create children to print their pid 1000 times
   for(int f=0; f<10;f++){
    int pid = fork();
    if (pid == 0){//if 0, that means that we are in the child process
        for (int i = 0; i < 1000; ++i){
		printf(1, "%d : %d \n", getpid(), i);
		
		}

        exit();
    } 
    }
    
    struct timeVariables *tv = malloc(sizeof(struct timeVariables));
    for(int f=0;f<10;f++){        	
        // Set the ptv variables after one of the children's work is finished
	ptv[f].pid = waitForChild(tv); 
        ptv[f].turnAroundTime = tv->terminationTime - tv->creationTime;
        ptv[f].CBT = tv->runningTime;
        ptv[f].waitingTime = tv->sleepingTime + tv->readyTime;
        
        // Update the atv variablles
        atv.averageTurnAroundTime += ptv[f].turnAroundTime;
        atv.averageCBT += ptv[f].CBT;
        atv.averageWaitingTime += ptv[f].waitingTime;
        /*printf(1, "pid %d create %d term %d ready %d sleep %d cbt %d \n"
        , ptv[f].pid, tv->creationTime, tv->terminationTime,
         tv->readyTime, tv->sleepingTime, tv->runningTime);*/
        /*printf(1, "creationTime %d\n", tv->creationTime);
        printf(1, "terminationTime %d\n", tv->terminationTime);
        printf(1, "readyTime %d\n", tv->readyTime);
        printf(1, "sleepingTime %d\n", tv->sleepingTime);
        printf(1, "runningTime %d\n \n", tv->runningTime);*/
        }
        
    // Print the required time variables of the children
    for(int i=0; i<10;i++){
    	printf(1,"Pid %d Turnaround time %d, CBT %d, and Waiting time %d .\n",ptv[i].pid, ptv[i].turnAroundTime, ptv[i].CBT, ptv[i].waitingTime);
    }

    // Print the average time variables of all of the children 
    printf(1, "Average Turnaround time %d, Average CBT %d, and Average Waiting time %d .\n", (atv.averageTurnAroundTime / 10), (atv.averageCBT / 10), (atv.averageWaitingTime / 10));
    exit();
}
