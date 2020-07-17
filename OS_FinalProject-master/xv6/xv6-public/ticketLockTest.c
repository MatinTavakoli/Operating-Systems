//
// Created by arash on 2020-01-24.
//testing ticket lock!

#include "types.h"
#include "user.h"
#include "stat.h"

#define NCHILD 10

int main ()
{
    int pid;
    ticketlockInit();//initializes ticket lock
    pid = fork();
    for (int i = 1; i < NCHILD; i++) {
        if (pid < 0){
            printf(1, "fork failed\n");
            exit();
        } else if (pid > 0){
            pid = fork();
        }
    }
    if (pid < 0){
        printf(1, "fork failed\n");
        exit();
    } else if (pid == 0){
        printf(1, "child adding t shared counter\n");
        printf(1,"%d\n",ticketlockTest());//prints value of ticket lock
        exit();
    } else{
        for (int i = 0; i < NCHILD; i++) {//waits for the forked processes to exit
            wait();
        }
        printf(1, "user program finished\n");
        printf(2, "ticket lock value: %d\n", ticketlockTest() - 1);//prints the most recent ticket lock value
    }
    exit();

    return 0;
}
