//
// Created by arash on 2020-01-24.
//

#include "types.h"
#include "user.h"
#include "stat.h"
#define NCHILD 10

void testReaderWriters(int* pattern, int pattern_size);
int main () {
    char argv[100];

    printf(1, "enter pattern for readers/writers test\n");
    int read_size;
    read_size = read(0, argv, sizeof(argv));//reading pattern
    argv[read_size - 1] = '\0';
    int pattern[100], i;
    for (i = 0; argv[i + 1] != '\0'; i++) {
        if (argv[i + 1] == '0')
            pattern[i] = 0;
        else if (argv[i + 1] == '1')
            pattern[i] = 1;
        else {
            printf(1, "pattern must consist of only 0 and 1");
            exit();
        }
    }
    testReaderWriters(pattern, i);
    exit();
    return 0;
}
void testReaderWriters(int* pattern, int pattern_size){
    int pid, i, res;
    rwinit();//initializes reader writer
    pid = fork();
    for (i = 1; i < pattern_size; i++){
        if (pid <0){
            printf(1, "fork failed\n");
            exit();
        } else if (pid > 0){
            pid = fork();
        } else {
            break;
        }
    }
    if (pid < 0){
        printf(1, "fork failed\n");
        exit();
    } else if (pid == 0) {
        printf(1, "child adding to shared counter %d\n", pattern[i-1]);
        res = rwtest(pattern[i-1]);
        if (pattern[i-1] == 0){//reader part
            printf(1, "reader read from shared counter\n");
        } else{//writer part
            printf(2, "writer added to shared counter\n");
        }
        exit();
    } else{
        for (int i = 0; i < pattern_size; i++) {//wait for the forked processes to exit
            wait();
        }
        printf(1, "user program finished\n");
        res = rwtest(0);
        printf(1, "last value of shared counter : %d\n", res);//prints last value of shared counter
    }
    exit();
}
