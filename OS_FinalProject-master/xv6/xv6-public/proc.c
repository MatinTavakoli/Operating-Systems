#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

struct ticketlock tl;
struct ticketlock mutex, write;
//struct spinlock mutex, write;

int nextpid = 1;
int nexttid = 1;
int sharedCounter = 0;
int readerCount=0;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static void wakeup1TicketLock(int pid);

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

//current proc
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

//current thread
// Disable interrupts so that we are not rescheduled
// while reading thread from the cpu structure
struct thread*
mythread(void) {
    struct cpu *c;
    struct thread *t;
    pushcli();
    c = mycpu();
    t = c->thread;
    popcli();
    return t;
}

// This function is for debugging
const char* getstate(enum threadstate tstate)
{
    switch (tstate)
    {
        case NOTUSED: return "notused";
        case EMBRYO: return "embryo";
        case SLEEPING: return "sleeping";
        case RUNNABLE: return "runnable";
        case RUNNING: return "running";
        case TZOMBIE:    return "thread zombie";
            //case ZOMBIE: return "zombie";
            /* etc... */
    }
    return "none";
}

// This function is for debugging
const char* getpstate(enum procstate pstate)
{
    switch (pstate)
    {
        case UNUSED: return "unused";
        case USED: return "used";
        case ZOMBIE: return "zombie";
            //case ZOMBIE: return "zombie";
            /* etc... */
    }
    return "none";
}

// Look in the process's thread table for an UNUSED thread.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct thread*
allocthread(struct proc *p) {
//    int i;
    struct thread *t;
    char *sp;
    //cprintf("%d\n", nextpid);

    acquire(&p->ttable.lock);

    /*   for (i = 0; i < MAX_THREADS; i++) {
           t = p->ttable.allthreads[i];
           if (t->tstate == NOTUSED)
               goto found;
       }*/

    for(t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++)//iterating through the process's thread table
        if(t->tstate == NOTUSED)
            goto found;
    return 0;

    found:
    t->tstate = EMBRYO;
    t->tid = nexttid++;
    t->tproc = p;
    //cprintf("%d %d\n", p->ttable.allthreads[0].tid, p->ttable.allthreads[0].tproc->pid);
    //p->ttable.allthreads[0] = t;
    release(&p->ttable.lock);

    //release(&ptable.lock);
    //release(p->ttable.lock);

    // Allocate kernel stack.
    if((t->kstack = kalloc()) == 0){
        t->tstate = NOTUSED;
        return 0;
    }
    sp = t->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *t->tf;
    t->tf = (struct trapframe*)sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint*)sp = (uint)trapret;

    sp -= sizeof *t->context;
    t->context = (struct context*)sp;
    memset(t->context, 0, sizeof *t->context);
    t->context->eip = (uint)forkret;

    return t;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
    struct proc *p;
//  struct spinlock lock;

    //char *sp;
//    cprintf("1\n");
    acquire(&ptable.lock);
//  cprintf("2\n");

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED)
            goto found;

    release(&ptable.lock);
    return 0;

    found:
    p->state = USED;
    p->pid = nextpid++;
//  p->ttable.lock = &lock;
    initlock(&p->ttable.lock, "ttable");
//  cprintf("%s\n", p->ttable.lock->name);

    //p->ttable->allthreads = t;
    //p->ttable->nexttid = 0;
    //release(&ptable.lock);
    allocthread(p);
    p->numOfThreads = 1;
    //cprintf("tid is %d\n", p->ttable.allthreads[0]);
//  cprintf("allocthread \n");
    release(&ptable.lock);
//  p->ttable->allthreads[0] = allocthread(p);
    //p->ttable->allthreads[p->ttable->nexttid] = allocthread(p);
    //p->ttable->nexttid += 1;

//  release(&ptable.lock);

/*  // Allocate kernel stack.
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
  p->context->eip = (uint)forkret;*/

    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
    //  cprintf("-1\n");
    struct proc *p;
    //cprintf("-2\n");
    struct thread *t;
    extern char _binary_initcode_start[], _binary_initcode_size[];
    //cprintf("0\n");
    p = allocproc();
    t = &p->ttable.allthreads[0];
    //cprintf("1\n");

    initproc = p;
    //cprintf("2\n");
    if((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    //cprintf("3\n");
    inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
    //cprintf("4\n");
    p->sz = PGSIZE;
    //cprintf("5\n");
    memset(t->tf, 0, sizeof(*t->tf));
    //cprintf("6\n");
//  p->ttable->allthreads[0]->tf->cs = (SEG_UCODE << 3) | DPL_USER;
//  p->ttable->allthreads[0]->tf->ds = (SEG_UDATA << 3) | DPL_USER;
//  p->ttable->allthreads[0]->tf->es = p->ttable->allthreads[0]->tf->ds;
//  p->ttable->allthreads[0]->tf->ss = p->ttable->allthreads[0]->tf->ds;
//  p->ttable->allthreads[0]->tf->eflags = FL_IF;
//  p->ttable->allthreads[0]->tf->esp = PGSIZE;
//  p->ttable->allthreads[0]->tf->eip = 0;  // beginning of initcode.S
    t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    //cprintf("11\n");
    t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    //cprintf("12\n");
    t->tf->es = t->tf->ds;
    //cprintf("13\n");
    t->tf->ss = t->tf->ds;
    //cprintf("14\n");
    t->tf->eflags = FL_IF;
    //cprintf("15\n");
    t->tf->esp = PGSIZE;
    //cprintf("16\n");
    t->tf->eip = 0;  // beginning of initcode.S
    //cprintf("7\n");

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");
//  cprintf("8\n");

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);
//  cprintf("9\n");

    p->state = USED;
    t->tstate = RUNNABLE;

    release(&ptable.lock);
    //cprintf("10\n");
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
    switchuvm(mythread());
    return 0;
}

//Creating a Thread for process
int
sys_createThread()
{
    void(*function)();
    void * stack;
    int tid;
    argptr(0, (void *)&function , sizeof(*function));
    argptr(1, (void *)&stack, sizeof(*stack));
    struct thread *nt;
    struct proc *curproc = myproc();
    struct thread *curthread = mythread();

    // Allocate process.
    if((nt = allocthread(curproc)) == 0){
        return -1;
    }

    nt->tproc = curproc;
//    nt->tparent = curthread;
    *nt->tf = *curthread->tf;


//    cprintf("%d %d %d\n", (uint) stack, (uint) function, nt->tid);
    nt->tf->eip = (uint)function;//extended instruction pointer
   // nt->context->eip = (uint) function;
    nt->tf->esp = (uint)stack;//extended stack pointer
//    nt->context->eip = (uint)function;
//    nt->context->esi = (uint)stack;

//    cprintf("%d %d %d %s \n", (uint)nt->tf->esp, nt->tf->eip, nt->tid,getstate(nt->tstate));

    // Clear %eax so that fork returns 0 in the child.
    nt->tf->eax = 0;
//    safestrcpy(nt->name, curthread->name, sizeof(curthread->name));

    tid = nt->tid;

    acquire(&ptable.lock);

//    cprintf("%s\n", getpstate(curproc->state));
//    nt->tproc->state = USED;
    nt->tstate = RUNNABLE;
    nt->tproc->numOfThreads += 1;

    release(&ptable.lock);
//    cprintf("create bye \n");
    setupkvm();
    return tid;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
    int i, pid;
    //cprintf("hi im fork \n");
    struct proc *np;
    struct proc *curproc = myproc();
    struct thread *curthread = mythread();
    struct thread *t;

    // Allocate process.
    if((np = allocproc()) == 0){
//      cprintf("fork started \n");
        return -1;
    }

//    cprintf("AHGSHG: %d \n", np->ttable.allthreads[0].kstack);

    // Copy process state from proc.
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
        np->state = UNUSED;
        acquire(&np->ttable.lock);
        for(t = np->ttable.allthreads; t < &np->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
            kfree(t->kstack);
            t->kstack = 0;
            t->tstate = NOTUSED;
            //np->ttable->allthreads[0]->tstate = NOTUSED;
        }
        release(&np->ttable.lock);
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    np->numOfThreads = 1;
    np->ttable.allthreads[0].tparent = curthread;
    for(t = np->ttable.allthreads; t < &np->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
        if (np->ttable.allthreads[0].tparent == curthread)
            continue;
        kfree(t->kstack);
        t->kstack = 0;
        t->tf = 0;
        t->tstate = NOTUSED;
        t->tid = 0;
        t->tparent = 0;
        //np->ttable->allthreads[0]->tstate = NOTUSED;
    }
    *np->ttable.allthreads[0].tf = *curthread->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->ttable.allthreads[0].tf->eax = 0;

    for(i = 0; i < NOFILE; i++)
        if(curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;
//  cprintf("1\n");
//    acquire(&ptable.lock);
//  cprintf("2\n");

    np->state = USED;
    np->ttable.allthreads[0].tstate = RUNNABLE;

  //  release(&ptable.lock);
//  cprintf("fork ended...\n");

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
//    cprintf("exit bye\n");
    struct proc *curproc = myproc();
//    struct thread *curthread = mythread();
    struct thread *t;
    struct proc *p;
    int fd;

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
//    cprintf("1\n");
    acquire(&ptable.lock);
//  cprintf("2\n");
//    acquire(&curproc->parent->ttable.lock);
    // Parent might be sleeping in wait().
    for (t = curproc->parent->ttable.allthreads;  t< &curproc->parent->ttable.allthreads[MAX_THREADS]; t++) {//iterating through the process's thread table
        wakeup1(t->chan);
    }
//    release(&curproc->parent->ttable.lock);

//    acquire(&initproc->ttable.lock);
    // Pass abandoned children to init.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//      cprintf("exit %d %d\n", p->parent->pid, curproc->pid);
        if(p->parent == curproc) {
            p->parent = initproc;
            if (p->state == ZOMBIE) {
                for (t = initproc->ttable.allthreads; t < &initproc->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
                    wakeup1(t->chan);
            }
            }
        }
    }
//    release(&initproc->ttable.lock);

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    // p->ttable->nexttid -= 1;
    acquire(&curproc->ttable.lock);
    for (t = curproc->ttable.allthreads; t < &curproc->ttable.allthreads[MAX_THREADS]; t++) {//iterating through the process's thread table
        if (t->tstate == NOTUSED)
            continue;
        t->tstate = TZOMBIE;
        t->tproc->numOfThreads = 0;
    }
    release(&curproc->ttable.lock);
//    curthread->tproc->numOfThreads = 0;
//    p->state = NOTUSED;
    sched();
    panic("zombie exit");
}

// Exit the current thread.  Does not return.
// An exited thread remains in the zombie state
// until its parent calls wait() to find out it exited.
void
sys_exitThread(void)
{
    struct thread *curthread = mythread();
    int numOfThread;
    curthread->tproc->numOfThreads -= 1;
    numOfThread = curthread->tproc->numOfThreads;
    acquire(&ptable.lock);

    if(curthread == &initproc->ttable.allthreads[0])
        panic("Thread init exiting");
    if (numOfThread == 0)
        exit();
    curthread->tstate = TZOMBIE;
    wakeup1(curthread->chan);
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
//    cprintf("wait\n");
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();
    struct thread *t;

    acquire(&ptable.lock);
    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//        cprintf("%d %d\n", p->parent->pid, curproc->pid);
            if(p->parent != curproc)
                continue;
            havekids = 1;
//      cprintf("1\n");
            //acquire(&p->ttable.lock);
//      cprintf("2\n");
                if(p->state == ZOMBIE){
                    // Found one.
                    freevm(p->pgdir);
                    pid = p->pid;
                    p->pid = 0;
                    p->parent = 0;
                    p->name[0] = 0;
                    p->killed = 0;
                    p->state = UNUSED;

                    acquire(&p->ttable.lock);
                    for(t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
                        //t = p->ttable.allthreads[i];
                        if (t->tstate == NOTUSED)
                            continue;
//                        cprintf("wait  %d %d %d %s\n",  t->tid, havekids, t->tproc->killed, getstate(t->tstate));
                        if (t->tstate == TZOMBIE) {
                            //cprintf("%s \n", getstate(t->tstate));
                            // Found one.
                            kfree(t->kstack);
                            t->kstack = 0;
                            // p->ttable->nexttid -= 1;
                            t->tid = 0;
                            t->tparent = 0;
                            t->tstate = NOTUSED;
                        }
                    }
                    release(&p->ttable.lock);
                    release(&ptable.lock);
                    return pid;
                }
            }

        // No point waiting if we don't have any children.
        if(!havekids || curproc->killed){
            release(&ptable.lock);
//      release(&p->ttable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(mythread()->chan, &ptable.lock);  //DOC: wait-sleep
//    cprintf("sleep");
    }
}

/*int
sys_joinThread(void)
{
    //    cprintf("wait\n");
//    struct proc *p;
    int founded;
    int threadID;
    struct proc *curproc = myproc();
    struct thread *t;
    argint(0, &threadID);

    acquire(&ptable.lock);
    for(;;){
        // Scan through table looking for exited children.
        founded = 0;
//        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//            if(p != curproc)
//                continue;
//        cprintf("%d %d\n", p->parent->pid, curproc->pid);
//      cprintf("1\n");
            //acquire(&p->ttable.lock);
//      cprintf("2\n");
//            acquire(&p->ttable.lock);
            for(t = curproc->ttable.allthreads; t < &curproc->ttable.allthreads[MAX_THREADS]; t++) {

                //cprintf("%d\n", t->tid);
                //t = p->ttable.allthreads[i];
                if (t->tid != threadID) {
                    if (!founded) {
                        founded = 0;
                    }
//                    release(&p->ttable.lock);
                    continue;
                }
                founded = 1;
                cprintf("join %d %d %d %d %s\n", threadID, t->tid, founded, t->tproc->killed, getstate(t->tstate));

                if (t->tstate == TZOMBIE) {
//                    release(&p->ttable.lock);
                    //cprintf("join %d %d\n", threadID, t->tid);
                    //release(&p->ttable.lock);
                    release(&ptable.lock);
                    return 0;
                }

            }
            // No point waiting if we don't have any children.

//            }
//        cprintf("founded is %d\n", founded);
        if(!founded || t->tproc->killed){
                cprintf("%d %d\n", threadID, mythread()->tid);
            release(&ptable.lock);
            if(!founded || t->tproc->killed){
                //cprintf("%d %d\n", threadID, mythread()->tid);
                release(&ptable.lock);
//                    release(&p->ttable.lock);
            return -1;
        }
        cprintf("sleep\n");
        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(mythread()->chan, &ptable.lock);  //DOC: wait-sleep




//    cprintf("sleep");
    }

}*/

//waits until requested thread exits
int
sys_joinThread(void)
{
    struct thread *t;
    int threadID;
    int havekids;//, tid;
    struct proc *curproc = myproc();
    argint(0, &threadID);

    acquire(&ptable.lock);
    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        for(t = curproc->ttable.allthreads; t < &curproc->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
            if(t->tid != threadID)
                continue;
            havekids = 1;
//            cprintf("join %d %d %d %d %s\n", threadID, t->tid, havekids, t->tproc->killed, getstate(t->tstate));
            if(t->tstate == TZOMBIE){//if we found it
//                cprintf("ok\n");
                // Found one.
//                tid = t->tid;
                kfree(t->kstack);//free memory space
                t->kstack = 0;
                t->tid = 0;
                t->tparent = 0;
                t->tstate = UNUSED;
                release(&ptable.lock);
                return 0;
            }
        }

        // No point waiting if we don't have any children.
//        cprintf("is %d %d \n", havekids, curproc->killed);
        if(!havekids || curproc->killed){
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(mythread()->chan, &ptable.lock);  //DOC: wait-sleep
    }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
    struct proc *p;
    struct thread *t;
    struct cpu *c = mycpu();
    c->proc = 0;

    for(;;){
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//        cprintf("%s", p->state);
            if(p->state != USED)
                continue;

            //cprintf("pid is %d\n", p->pid);

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            /*c->proc = p;
            switchuvm(p);*/
            //p->state = RUNNING;
            //cprintf("beforelock\n");
            acquire(&p->ttable.lock);
            //cprintf("afterlock\n");
            c->proc = p;
            for(t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table

                //t = p->ttable.allthreads[i];
                if(t->tstate != RUNNABLE)
                    continue;
//                cprintf("%s %d %d\n", getstate(p->state), p->pid, t->tid);

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                //c->proc = 0;
                //cprintf("byo\n");
                c->thread = t;
//                cprintf("%d\n", t->tid);
                t->tstate = RUNNING;
                release(&p->ttable.lock);
                switchuvm(t);
//                cprintf("%d %s\n", t->tid, getstate(t->tstate));


                // Switch to chosen process.  It is the process's job
                // to release ptable.lock and then reacquire it
                // before jumping back to us.
                swtch(&(c->scheduler), t->context);
//            cprintf("1\n");
                acquire(&p->ttable.lock);
//            cprintf("2\n");
                switchkvm();
                //cprintf("halo\n");

                c->thread = 0;
                //cprintf("release cpu\n");
            }
            release(&p->ttable.lock);
            //cprintf("release lock \n");
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            //cprintf("bye cpu\n");
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
//  cprintf("sched is here\n");
    //struct proc *p = myproc();
    //cprintf("%s\n", getstate(mythread()->tstate));
    struct thread *t = mythread();
//  cprintf("thread say hello\n");
//  acquire(&t->tproc->ttable.lock);
    if(!holding(&ptable.lock))
        panic("sched ptable.lock");
    if(mycpu()->ncli != 1)
        panic("sched locks");
    if(t->tstate == RUNNING)
        panic("sched running");
    if(readeflags()&FL_IF)
        panic("sched interruptible");


    intena = mycpu()->intena;
//  cprintf("intena\n");
    swtch(&t->context, mycpu()->scheduler);
//  cprintf("swtch\n");
    mycpu()->intena = intena;
//  cprintf("bye sched\n");
//  cprintf("%s\n", getstate(t->tstate));
//    release(&t->tproc->ttable.lock);
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
//    cprintf("yieldi!!!|\n");
    acquire(&ptable.lock);  //DOC: yieldlock
    //myproc()->state = RUNNABLE;
    mythread()->tstate = RUNNABLE;
//  cprintf("yield!!!\n");
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
//    cprintf("halo im forkret\n");
    static int first = 1;
    // Still holding ptable.lock from scheduler.
//  release(myproc()->ttable.lock);
    release(&ptable.lock);
//    cprintf("from forkret %d \n", first);
    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
//    cprintf("first passed\n");
        iinit(ROOTDEV);
//    cprintf("next step\n");
        initlog(ROOTDEV);
    }
//    cprintf("bye from forkret\n");

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
    //struct proc *p = myproc();
//  cprintf("sleepingg!!!\n");
    struct thread *t = mythread();
    //cprintf("%d \n",t->tid);

    if(t == 0)
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
    t->chan = chan;
    t->tstate = SLEEPING;
//  cprintf("watch out sched is here!!!\n");
    sched();
//  cprintf("bye schedy!!!\n");

    // Tidy up.
    t->chan = 0;

    // Reacquire original lock.
    if(lk != &ptable.lock){  //DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
//  cprintf("wakeup now!!!\n");
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
    struct proc *p;
    struct thread *t;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        acquire(&p->ttable.lock);
        for(t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
            if (t->tstate == SLEEPING && t->chan == chan)
                t->tstate = RUNNABLE;
        }
        release(&p->ttable.lock);
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

//PAGEBREAK!
// Wake up process with the specific  pid.
// The ptable lock must be held.
static void
wakeup1TicketLock(int pid)
{
    struct proc *p;
    struct thread *t;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            acquire(&p->ttable.lock);
            for(t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++){//iterating through the process's thread table
                if (t->tstate == SLEEPING)
                    t->tstate = RUNNABLE;
            }
            release(&p->ttable.lock);
        }
    }

}

// Wake up  process with specific pid.
void
wakeupTicketLock(int pid)
{
    acquire(&ptable.lock);
    wakeup1TicketLock(pid);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
    struct proc *p;
    struct thread *t;
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->pid == pid){
            p->killed = 1;
            // Wake process from sleep if necessary.
            // Wake threads from sleep if necessary.
            acquire(&p->ttable.lock);
            for (t = p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS];t++) {//iterating through the process's thread table
                if (t->tstate == SLEEPING)
                    t->tstate = RUNNABLE;
            }
            release(&p->ttable.lock);
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
            [NOTUSED]   "notused",
            [EMBRYO]    "embryo",
            [SLEEPING]  "sleeping",
            [RUNNABLE]  "runnable",
            [RUNNING]   "running",
            [TZOMBIE]    "thread zombie"
    };
    int i;
    struct proc *p;
    struct thread *t;
    char *state;
    uint pc[10];

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
//      cprintf("1\n");
        acquire(&p->ttable.lock);
//      cprintf("2\n");

        for (t=p->ttable.allthreads; t < &p->ttable.allthreads[MAX_THREADS]; t++) {//iterating through the process's thread table
            if (t->tstate == NOTUSED)
                continue;
            if (t->tstate >= 0 && t->tstate < NELEM(states) && states[t->tstate])
                state = states[t->tstate];
            else
                state = "???";
            cprintf("%d %s %s", p->pid, state, p->name);
            if (t->tstate == SLEEPING) {
                getcallerpcs((uint *) t->context->ebp + 2, pc);
                for (i = 0; i < 10 && pc[i] != 0; i++)
                    cprintf(" %p", pc[i]);
            }
            cprintf("\n");
        }
        release(&p->ttable.lock);
    }
}

int
sys_ticketlockInit(void)
{
    sharedCounter = 0;
    initticketlock(&tl,"ticketLock");
    return 0;
}
int
sys_ticketlockTest(void)
{
    acquireticketlock(&tl);
    microdelay(700000);
    sharedCounter++;
    releaseticketlock(&tl);
    return tl.ticket;
}

int
sys_rwinit(void)
{
    sharedCounter = 0;
    readerCount = 0;
    initticketlock(&mutex, "mutex readerwriter");
    initticketlock(&write, "write readerwrite");
    /*initlock(&mutex, "m");
    initlock(&write, "w");*/
    return 0;
}

int
sys_rwtest(void)
{
    int pattern;
    int result = 0;
    argint(0, &pattern);
    // Writer
    if (pattern == 1) {
        acquireticketlock(&write);
        microdelay(70);
        sharedCounter++;
        releaseticketlock(&write);
    }
        // Reader
    else if (pattern == 0){
        acquireticketlock(&mutex);
        readerCount++;
        if (readerCount == 1){
            acquireticketlock(&write);
        }
        releaseticketlock(&mutex);
        result = sharedCounter;
        microdelay(70);
        acquireticketlock(&mutex);
        readerCount--;
        if (readerCount == 0){
            releaseticketlock(&write);
        }
        releaseticketlock(&mutex);
    }
    //releaseticketlock(&tl)
    return result;
}

/*int
sys_rwtest(void)
{
    int pattern;
    int result = 0;
    argint(0, &pattern);
    // Writer
    if (pattern == 1) {
        acquire(&write);
        microdelay(1000000);
        sharedCounter++;
        release(&write);
    }
        // Reader
    else if (pattern == 0){
        acquire(&mutex);
        readerCount++;
        if (readerCount == 1){
            acquire(&write);
        }
        release(&mutex);
        result = sharedCounter;
        microdelay(700000);
        acquire(&mutex);
        readerCount--;
        if (readerCount == 0){
            release(&write);
        }
        release(&mutex);
    }
    //releaseticketlock(&tl)
    return result;
}*/
