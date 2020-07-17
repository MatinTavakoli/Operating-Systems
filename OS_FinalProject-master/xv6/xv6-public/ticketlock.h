// Mutual exclusion lock.
struct ticketlock {
    uint locked;           // Is the lock held?
    struct spinlock lk;   // spinlock protecting this sleep lock
    int ticket;
    int waitedPid[100];  // Array Of Waited Process
    int QHead;          // Head Of The Queue
    int QTail;         // Tail Of The Queue


    // For debugging:
    char *name;        // Name of lock.
    int pid;          // Process holding lock
    // that locked the lock.
};

