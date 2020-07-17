#include "types.h"
#include "stat.h"
#include "user.h"
 
int
main(void)
{
int pid, pid1, pid2;
pid = fork();
pid1 = fork();
pid2 = fork();

if(pid == 0)
   {
      while(1);
      exit();
	}
else if(pid > 0)
   {
    if (pid1 == 0)
      {
        while(1);
        exit();
        }
    else if( pid1 > 0)
       {
        if(pid2 == 0)
        	{
        	   while(1);
        	   exit();
        	   }
        else if(pid2 > 0)
        	{
        	    getpid();
        	    getppid();
        	    getppid();
		    printf(1,"The System Call Fork() Invoked %d Times. \n", getCount(1));
		    printf(1,"The System Call getpid() Invoked %d Times. \n", getCount(11));
		    printf(1,"The System Call getppid() Invoked %d Times. \n", getCount(22));
                    exit();
		}
	}

   }        	
        	
return 0;
}
