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
   struct processTimeVariables ptv[5][5] ;
   struct averageTimeVariables atv[5];
   int numOfProc [5];
   
   // Reinitialize the averageTimeVariables valuse
   for(int i=0;i<5;i++){
       atv[i].averageTurnAroundTime = 0;
       atv[i].averageCBT = 0;
       atv[i].averageWaitingTime = 0;
   }
   
   // Change The Scheduling Algorithm To The Priority 
   changePolicy(2); 

   // Create childrens to print their pid 1000 times and assign their priority to them
   for(int f=0; f<25;f++){
    int pid = fork();

    if (pid == 0){    
    	changePriority(1 + getpid() % 5);
        for (int i = 0; i < 500; ++i){
		printf(1, "%d : %d \n", getpid(), i);
		}

        exit();
    } 
    }
    
    struct timeVariables *tv = malloc(sizeof(struct timeVariables));
    for(int f=0;f<25;f++){
    	// Get the pid of the finished child        	
	int pid = waitForChild(tv);
	// Find the priority of the child
	int i = pid % 5;
	
	// Set the ptv variables after one childrens work finished
	ptv[i][numOfProc[i]].pid = pid; 
        ptv[i][numOfProc[i]].turnAroundTime = tv->terminationTime - tv->creationTime;
        ptv[i][numOfProc[i]].CBT = tv->runningTime;
        ptv[i][numOfProc[i]].waitingTime = tv->sleepingTime + tv->readyTime;
        
        // Update the atv variablles
        atv[i].averageTurnAroundTime += ptv[i][numOfProc[i]].turnAroundTime;
        atv[i].averageCBT += ptv[i][numOfProc[i]].CBT;
        atv[i].averageWaitingTime += ptv[i][numOfProc[i]].waitingTime;
        
        // Update the number of the process that are in one priority
        numOfProc[i]+=1;
        /*printf(1, "pid %d create %d term %d ready %d sleep %d cbt %d \n"
        , ptv[f].pid, tv->creationTime, tv->terminationTime,
         tv->readyTime, tv->sleepingTime, tv->runningTime);*/
        /*printf(1, "creationTime %d\n", tv->creationTime);
        printf(1, "terminationTime %d\n", tv->terminationTime);
        printf(1, "readyTime %d\n", tv->readyTime);
        printf(1, "sleepingTime %d\n", tv->sleepingTime);
        printf(1, "runningTime %d\n \n", tv->runningTime);*/
        }
        
    // Print the required times of the childrens
    for(int i=0; i<5;i++){
    	for(int j=0;j<5;j++){
    		printf(1,"Pid %d , Priority %d, Turnaround time %d, CBT %d, and Waiting time %d .\n"
    		, ptv[i][j].pid, (i+1),ptv[i][j].turnAroundTime, ptv[i][j].CBT, ptv[i][j].waitingTime);
    	}
    }
    
    // Print the average time variables of the prioritys 
    for(int i=0;i<5;i++){
    	printf(1, "Process With Priority %d ,Average Turnaround time %d, Average CBT %d, and Average Waiting time %d .\n"
    	, (i+1), (atv[i].averageTurnAroundTime / 10), (atv[i].averageCBT / 10), (atv[i].averageWaitingTime / 10));
    	}
    exit();
}
