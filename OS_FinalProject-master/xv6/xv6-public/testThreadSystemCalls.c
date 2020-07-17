//
// Created by arash on 2020-01-24.
//testing thread system calls!

#include "types.h"
#include "user.h"
#include "stat.h"

#define NUMOFTHREADS 7

//functoin that every thread has to execute(a simple print and sleep, followed by exiting the thread)
void threadFunction(){
	printf(1, "Thread ID is  %d \n", getThreadID());
	sleep(100);
	exitThread();
}


//int main ()
//{
//    int pid;
//    int tid = 0;
//    int allThreadstID [NUMOFTHREADS];
//    for (int i=0; i< NUMOFTHREADS; i++){
//        tid++;
//        sleep(5);
//        allThreadstID[i] = createThread(&threadFunction, malloc(4096));
//    }    pid = fork();
//
//    if (pid < 0){
//        printf(1, "fork failed\n");
//        exit();
//    } else if (pid == 0){
//        printf(1, "child adding t shared counter\n");
//        printf(1, "before join\n");
//        for (int i = 0; i < NUMOFTHREADS; i++) {
//            sleep(70);
//            printf(1,"%d\n",joinThread(allThreadstID[i]));
//        }
//        printf(1, "after join \n");
//        exit();
//    } else{
//        printf(1,"before wait \n");
//        wait();
//        printf(1,"after wait %d\n", tid);
//    }
//    exit();
//
//    return 0;
//}

int main ()
{

    int allThreadstID [NUMOFTHREADS];//array of threads
    int joinstatus;
    for (int i=0; i< NUMOFTHREADS; i++){
        sleep(5);
        allThreadstID[i] = createThread(&threadFunction, malloc(4096));//creating thread
    }

        printf(1, "before join\n");
        for (int i = 0; i < NUMOFTHREADS; i++) {
            sleep(70);

            joinstatus = joinThread(allThreadstID[i]);//waits till this thread exits and joins
//            printf(1,"%d\n",joinstatus);
//            sleep(7);
            if (!joinstatus){//success
                printf(1, "Thread %d joined successfully.\n", allThreadstID[i]);
            } else{//failure
                printf(1, "Thread %d joined failed. \n", allThreadstID[i]);
            }
        }
        printf(1, "after join \n");
        exit();

    return 0;
}

//testThreadSystemCalls
