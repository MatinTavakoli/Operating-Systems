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
                    int children = getChildren(getpid());
                    printf(1,"All Childrens Of %d Are %d .\n", getpid(), children);
                    exit();
		}
	}

   }        	
        	
return 0;
}

