// kernel/sched/wait.h — per-object wait lists.
//
// See docs/pillars/12-scheduling.md (wait queues section).
//
// Usage pattern (always with the surrounding condition's critical section held):
//
//     irq_disable();
//     while (!condition) {
//         wait_list_add(&obj->waiters, current);
//         scheduler_block();   // returns with `current` runnable again
//     }
//     // do work
//     irq_enable();
//
// On the waker side:
//
//     irq_disable();
//     // mutate state so `condition` becomes true
//     wait_list_wake_one(&obj->waiters);   // or _wake_all
//     irq_enable();

#ifndef SKL_KERNEL_SCHED_WAIT_H
#define SKL_KERNEL_SCHED_WAIT_H

#include "sched.h"

struct wait_list {
    struct thread* head;
    struct thread* tail;
};

#define WAIT_LIST_INIT { .head = NULL, .tail = NULL }

void wait_list_add(struct wait_list* wl, struct thread* t);
void wait_list_wake_one(struct wait_list* wl);
void wait_list_wake_all(struct wait_list* wl);

#endif
