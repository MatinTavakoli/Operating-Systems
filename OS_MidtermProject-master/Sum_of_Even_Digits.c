//
// Created by arash on 2019-12-05.
//
// C program to demonstrate use of fork() and pipe()
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>

int main()
{
    // We use two pipes
    // First pipe to send input number from parent
    // Second pipe to send sum of even digits from child

    int fd1[2]; // Used to store two ends of first pipe
    int fd2[2]; // Used to store two ends of second pipe

    char in_num[100];
    pid_t p;

    if (pipe(fd1)==-1)
    {
        fprintf(stderr, "Pipe Failed" );
        return 1;
    }
    if (pipe(fd2)==-1)
    {
        fprintf(stderr, "Pipe Failed" );
        return 1;
    }

    p = fork();

    if (p < 0)
    {
        fprintf(stderr, "fork Failed" );
        return 1;
    }

        // Parent process
    else if (p > 0)
    {
        scanf("%s", in_num);

        char even_sum[1];

        close(fd1[0]); // Close reading end of first pipe

        // Write input string and close writing end of first
        // pipe.
        write(fd1[1], in_num, strlen(in_num) + 1);
        close(fd1[1]);

        // Wait for child to send a string
        wait(NULL);

        close(fd2[1]); // Close writing end of second pipe

        // Read string from child, print it and close
        // reading ended.
        read(fd2[0], even_sum, 100);
        printf("%s The sum of even digits in the input number : %s\n", in_num, even_sum);
        close(fd2[0]);
    }

        // child process
    else
    {
        close(fd1[1]); // Close writing end of first pipe

        // Read a string using first pipe
        char parent_num[100];
        int even_sum = 0;
        int num_i;
        read(fd1[0], parent_num, 100);

        // Summing even digits
        int k = strlen(parent_num);
        int i;
        for (i=0; i<k; i++){
            num_i = (int)(parent_num[i]) - 48;
            if(num_i % 2 == 0){
                even_sum = even_sum + num_i;
            }
        }

        // Close both reading ends
        close(fd1[0]);
        close(fd2[0]);

        // Convert the sum of even to a string
        char s[100] ;
        sprintf(s, "%d", even_sum);

        // Write string and close writing end
        write(fd2[1], s, strlen(s) + 1);
        close(fd2[1]);

        exit(0);
    }
}

