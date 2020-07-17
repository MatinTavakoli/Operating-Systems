// Ticket locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

void
initticketlock(struct ticketlock *lk, char *name)
{
    initlock(&lk->lk, "ticket lock");
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
    lk->ticket = 0;
    lk->QHead = -1;
    lk->QTail = 0;
}

void
acquireticketlock(struct ticketlock *lk)
{
    acquire(&lk->lk);
       if (lk->locked) {
            lk->waitedPid[lk->QTail] = myproc()->pid;
            fetch_and_add(&lk->QTail, 1);
        }
        fetch_and_add(&lk->ticket, 1);
        if (lk->QTail == 100){
            lk->QTail  =0;//if tail reaches the end of the array, reset it to 0(like a circular queue)
        }
        while (lk->locked) {
            sleep(lk, &lk->lk);
        }
        lk->locked = 1;//lock acquired
        lk->pid = myproc()->pid;
        release(&lk->lk);

}

void
releaseticketlock(struct ticketlock *lk)
{
    acquire(&lk->lk);
    lk->locked = 0;//lock released
    lk->pid = 0;
    fetch_and_add(&lk->QHead, 1);
    if(lk->QHead == 100){
        lk->QHead = 0;//if head reaches the end of the array, reset it to 0(like a circular queue)
    }
    if (lk->QHead == lk->QTail){//if head and tail reach each other, reset head and tail
        lk->QHead = -1;
        lk->QTail = 0;
    }
   wakeupTicketLock(lk->waitedPid[lk->QHead]);
   release(&lk->lk);
}

/*int
holdingticket(struct ticketlock *lk)
{
    int r;

    acquire(&lk->lk);
    r = lk->locked && (lk->pid == myproc()->pid);
    release(&lk->lk);
    return r;
}
*/


