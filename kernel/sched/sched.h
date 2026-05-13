// kernel/sched/sched.h — scheduler interface.
//
// See docs/pillars/12-scheduling.md.

#ifndef SKL_KERNEL_SCHED_SCHED_H
#define SKL_KERNEL_SCHED_SCHED_H

#include <stdint.h>
#include <stddef.h>

struct thread;
struct process;

typedef enum {
    THREAD_NEW      = 0,
    THREAD_RUNNABLE = 1,
    THREAD_RUNNING  = 2,
    THREAD_BLOCKED  = 3,
    THREAD_EXITING  = 4,
    THREAD_DEAD     = 5,
} thread_state_t;

// The scheduler's view of a thread. Process-level info hangs off thread->proc.
struct thread {
    thread_state_t state;
    struct process* proc;

    // Scheduler bookkeeping.
    struct thread* run_next;        // intrusive runqueue linkage
    struct thread* run_prev;
    uint32_t       ticks_remaining; // time-slice countdown

    // Wait-list bookkeeping (one membership at a time in v1).
    struct thread* wait_next;
    struct thread* wait_prev;
    struct wait_list* wait_list;    // back-pointer for assertions

    // Arch-specific saved context.
    void* arch_ctx;
};

// The currently running thread. One global in v1 (single CPU).
extern struct thread* current;

// Bring up the scheduler at boot. Creates the idle thread.
void sched_init(void);

// Add a runnable thread to the runqueue (tail).
void sched_add(struct thread* t);

// Cooperative yield: move current to the runqueue tail, pick next.
void sched_yield(void);

// Block the current thread. Must be called with the relevant condition's
// critical section held (in v1, that means IRQs disabled). After this returns,
// the thread is RUNNING again; the caller re-checks the condition.
void scheduler_block(void);

// Timer tick handler — called by the per-arch timer IRQ.
void sched_tick(void);

#endif
