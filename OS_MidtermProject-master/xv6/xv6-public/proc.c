#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


static struct proc *initproc;

int nextpid = 1;
int schedNum = 0;//our default scheduling algorithm is the original xv6 scheduling algorithm.
int time = 0;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static struct proc *highestPriority(void);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}
/*returns the number of the highest priority based on the proceses' calculated priorities in the pttable*/
int
highestPriorityNumber(void)
{
  struct proc *p;
  int min = MAX;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE && p->calculatedPriority < min){
        min = p->calculatedPriority;
        }
} 
 return min;
}
//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  int min = highestPriorityNumber();
  char *sp;
  struct proc *p;
  acquire(&ptable.lock);
       
  //p = highestPriority();
  //min = p-> calculatedPriority;
  if (min == MAX){
  	min = 0;
  }
  //min = 1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->time_slot = 0;
  p->priority = 5;//default for new processes
  p->calculatedPriority = min;//default for new processes
  /*pushcli();
  p->creationTime = mycpu()->time_slot;
  popcli();*/
  p->creationTime = ticks;//marking creation time by the current value of ticks.
  p->runningTime = 0;
  p->getTheFirstCpu = 0;//hasn't acquired the CPU yet.
  p->sleepingTime = 0;
  p->readyTime = 0;
  p->terminationTime = 0;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);//swtching back to user mode
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}
/*resetting the counter*/
void reinitCounter(void){
	for(int i=0; i< 30;i++){
	     myproc()->counter[i] = 0;
	}
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
 /* pushcli();
  struct cpu *c = mycpu();
  popcli();*/
  int fd;
  reinitCounter();//when the process leaves, it must reset it's counter
  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE){
        wakeup1(initproc);
        }
    }
  }


  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;  
  //curproc->terminationTime = c->time_slot;
  curproc->terminationTime = ticks;
  //cprintf("pid %d create %d term %d ready %d sleep %d cbt %d \n", curproc->pid, curproc->creationTime, curproc->terminationTime, curproc->readyTime, curproc->sleepingTime, curproc->runningTime);
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}


struct proc* 
highestPriority(void){  
   int min = MAX;
   struct proc *minp = myproc();
   struct proc *p;
   /*pushcli();
   struct cpu *c = mycpu();
   popcli();*/

   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
     /* else if(p->state == EMBRYO){
      	 p->creationTime +=1;
      }
      else if(p->state == RUNNABLE && p->getTheFirstCpu == 0){
         p->readyTime += 1;
         }
      else if(p->state == SLEEPING){
       	p->sleepingTime+=1;
       	//cprintf("%d ready %d sleeping %d \n", p->pid, p->readyTime, p->sleepingTime);
       }
       c->time_slot += 1;*/
      if(p->state == RUNNABLE && p->calculatedPriority < min){
        min = p->calculatedPriority;
        minp = p;
        }
}        

return minp;
}


//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
/* 3.1 
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      p->time_slot += 1; // Increment Time slot
      if(p->time_slot == QUANTUM)
      {
      p->time_slot = 0;
      c->proc = p;
      switchuvm(p);//swtching back to user mode
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();//swtching to kernel mode
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
    release(&ptable.lock);
  }
}
*/

/* 3.2 
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
      p = highestPriority();
      if (p != myproc()){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      p->calculatedPriority += p->priority;
      c->proc = p;
      switchuvm(p);//swtching back to user mode
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();//swtching to kernel mode
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    release(&ptable.lock);
  }
}
*/
/*updating time variables based on the process state*/
void
calculateTime(void){
struct proc *p;
acquire(&ptable.lock);
for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	if(p->state == RUNNING){
		p->runningTime+=1;
	}
	else if(p->state == SLEEPING){
		p->sleepingTime+=1;
	}
	else if(p->state == RUNNABLE && p->getTheFirstCpu){
		p->readyTime +=1;
		}
	else if(p->state == EMBRYO){
		p->creationTime += 1;

	}
	}
release(&ptable.lock);

}


void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    if (schedNum == 0){//original xv6 algorithm

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      /*if(p->state != RUNNABLE){		
	      if(p->state == EMBRYO){
		p->creationTime+=1;
		      time+=1;
		}
     
	      else if(p->state == SLEEPING){
	       	p->sleepingTime+=1;
	       	p->runningTime += 1;
	       	      time+=1;
	       	//cprintf("%d ready %d sleeping %d \n", p->pid, p->readyTime, p->sleepingTime);
	       }
	       continue;
       }*/      //c->time_slot+=1;
      /*if(p->state == EMBRYO){
        p->creationTime+=1;
        }
      else if(p->state == SLEEPING){
       	p->sleepingTime+=1;
       	p->runningTime +=1;
       //	cprintf("%d ready %d sleeping %d \n", p->pid, p->readyTime, p->sleepingTime);
       }*/
       if(p->state != RUNNABLE)
        continue;	      
       /*if(p->state == RUNNABLE && p->getTheFirstCpu == 0){
		 //p->readyTime = c->time_slot - p->creationTime;
		 p->readyTime += 1;
		 p->runningTime =+1;
		 //p->runningTime += 1;
		 } */
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);//swtching back to user mode
	
      //p->runningTime += 1;
      p->getTheFirstCpu = 1;
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();//swtching to kernel mode

     // time+=1;
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      //cprintf("%d : %d\n", p->pid, ticks);

    }
    }
    else if (schedNum == 1){//modified xv6 algorithm
    
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){      

      if(p->state != RUNNABLE)
        continue;
        

      /*else if(p->state == EMBRYO){
        p->creationTime+=1;
        }
      else if(p->state == RUNNABLE && p->getTheFirstCpu == 0){
         p->readyTime += 1;
         }
      else if(p->state == SLEEPING){
       	p->sleepingTime+=1;
       //	cprintf("%d ready %d sleeping %d \n", p->pid, p->readyTime, p->sleepingTime);
       }*/
             /*if(p->state != RUNNABLE){		
	      if(p->state == EMBRYO){
		p->creationTime+=1;
		}
    
	      else if(p->state == SLEEPING){
	       	p->sleepingTime+=1;
	       	p->runningTime += 1;
	       	//cprintf("%d ready %d sleeping %d \n", p->pid, p->readyTime, p->sleepingTime);
	       }
	       continue;
       }
	      if(p->state == RUNNABLE && p->getTheFirstCpu == 0){
		 //p->readyTime = c->time_slot - p->creationTime;
		 p->readyTime += 1;
		 p->runningTime += 1;
		 } */ 
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.      
      //time+=1;
      //c->time_slot += 1; // Increment Time slot
      //cprintf("%d \n", p->pid);

      //if(ticks % QUANTUM == 0)
      //{
      //c->time_slot = 0;
      c->proc = p;
      switchuvm(p);//swtching back to user mode
	
     // p->runningTime += QUANTUM;
      p->getTheFirstCpu = 1;
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();//swtching to kernel mode
      //ticks++;
    
    //}
    
    
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    
    }
    else if(schedNum == 2){//priority based algorithm
      p = highestPriority();
      if (p != myproc()){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      p->calculatedPriority += p->priority;
      p->getTheFirstCpu = 1;
      c->proc = p;
      switchuvm(p);//swtching back to user mode
	
      //p->runningTime += 1;	
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();//swtching to kernel mode
      
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }


    }

    release(&ptable.lock);

  }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{ 
  //cprintf("%d : %d\n", myproc()->pid, ticks);
  if(schedNum == 1 && myproc()->runningTime % QUANTUM == 0){
	  acquire(&ptable.lock);  //DOC: yieldlock
	  myproc()->state = RUNNABLE;
	  sched();
	  release(&ptable.lock);
  }
  else if(schedNum == 0 || schedNum == 2){
    	  acquire(&ptable.lock);  //DOC: yieldlock
	  myproc()->state = RUNNABLE;
	  sched();
	  release(&ptable.lock);
  }
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Count Number Of Digits
int countDigit(int n)
{ 
    if (n == 0) 
    	{
        return 0;
        } 
    return 1 + countDigit(n / 10); 
} 

// return children pid
int
sys_getChildren(void)
{
struct proc *p;
int pid;
int res = 0;
int cons = 1;
argint(0, &pid);
 // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    
      	if(p->parent->pid == pid)
      	{
      		res = res + p->pid*cons;
      		for(int i=0; i<countDigit(p->pid); i++ )
      		{
      			cons = 10 * cons;
      		}
      		cprintf("pid is : %d ppid is : %d \n", p->pid, p->parent->pid);
      	}


    }
    release(&ptable.lock);

return res;

}

/*changes the scheduling algorithm*/
int
sys_changePolicy(void)
{
int n;
argint(0, &n);
if (n >= 0 && n < 3){
	schedNum = n;
	return 1;
	}
else {
	return -1;
	}
}

int
sys_waitForChild(void){
  struct timeVariables *time;
  argptr(0, (void*)&time, sizeof(*time));
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;

      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        time->creationTime = p->creationTime;
      	time->terminationTime = p->terminationTime;
      	time->sleepingTime = p->sleepingTime;
      	time->readyTime = p->readyTime;
      	time->runningTime = p->runningTime;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      time->creationTime = 0;
      time->terminationTime = 0;
      time->sleepingTime = 0;
      time->readyTime = 0;
      time->runningTime = 0;
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
